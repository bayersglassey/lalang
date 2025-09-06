#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "lalang.h"


/****************
* CODE
****************/

const char *instruction_name[N_INSTRUCTIONS] = {
    "LOAD_INT",
    "LOAD_STR",
    "LOAD_GLOBAL",
    "GETTER",
    "SETTER",
    "NEG",
    "ADD",
    "SUB",
    "MUL",
    "DIV",
    "MOD",
    "EQ",
    "NE",
    "LT",
    "LE",
    "GT",
    "GE",
    "CALL"
};

#define CODE_SIZE 1024

static void code_grow(code_t *code, int len) {
    // NOTE: the size of code->bytecodes is currently static, i.e. always CODE_SIZE
    // TODO: allow code to be dynamically resized :P
    if (len >= CODE_SIZE) {
        fprintf(stderr, "Can't grow code beyond max size: %i\n", CODE_SIZE);
        exit(1);
    }
    if (code->len >= len) return;
    if (!code->bytecodes) {
        bytecode_t *bytecodes = malloc(len * sizeof *bytecodes);
        if (!bytecodes) {
            fprintf(stderr, "Failed to allocate bytecodes\n");
            exit(1);
        }
        code->bytecodes = bytecodes;
    }
    code->len = len;
}

code_t *code_create() {
    code_t *code = calloc(1, sizeof *code);
    if (!code) {
        fprintf(stderr, "Failed to allocate code\n");
        exit(1);
    }
    return code;
}

code_t *code_push_instruction(code_t *code, instruction_t instruction) {
    code_grow(code, code->len + 1);
    code->bytecodes[code->len - 1].instruction = instruction;
}

code_t *code_push_i(code_t *code, int i) {
    code_grow(code, code->len + 1);
    code->bytecodes[code->len - 1].i = i;
}


/****************
* COMPILER
****************/

compiler_t *compiler_create(vm_t *vm) {
    compiler_t *compiler = calloc(1, sizeof *compiler);
    if (!compiler) {
        fprintf(stderr, "Failed to allocate memory for compiler\n");
        exit(1);
    }
    compiler->vm = vm;
    compiler->frame = compiler->frames - 1;
    return compiler;
}

static compiler_frame_t *compiler_push_frame(compiler_t *compiler) {
    compiler_frame_t *frame = ++compiler->frame;
    frame->code = code_create();
    return frame;
}

static char *get_token(char *text, int *token_len_ptr) {
    // Returns a pointer to the start of the token in text, or NULL if no
    // token found.
    // If token was found, also sets *token_len to token's length.
    // In that case, caller likely wants to do text = token + token_len
    // before trying to get another token.

    // First, consume whitespace & comments:
    bool comment = false;
    char c;
    while (c = *text) {
        if (c == ' ');
        else if (c == '#') comment = true;
        else if (c == '\n') comment = false;
        else if (!comment) break;
        text++;
    }
    if (!c) return NULL; // no token

    // Now, let's get a token:
    char *token = text;
    if (token[0] == '"') {
        // string literal
        // NOTE: absolutely any non-'"' character is allowed in here, even
        // newlines.
        // Escape next character with backslash.
        // Terminate with EOF, or non-escaped '"' or '\n'.
        text++;
        while (c = *text++) {
            if (c == '\\') {
                if (!(c = *text++)) break;
            } else if (c == '"' || c == '\n') break;
        }
        if (c != '"') text--; // make sure we preserve the terminating '"', if any
    } else {
        // not a string literal
        // Terminate with EOF, ' ', or '\n'.
        while (c = *text++) {
            if (c == ' ' || c == '\n') break;
        }
        text--;
    }
    if (!c) return NULL; // no token
    *token_len_ptr = text - token;
    return token;
}

static const char *parse_string_literal(const char *token, int token_len) {
    // NOTE: assumes token starts & ends with '"'.

    // Allocate at least enough to hold token, without the '"'s, plus a '\0'.
    // The parsed string's length will be token_len, minus the number of
    // escaping backslashes in token.
    char *parsed = malloc(token_len - 2 + 1);
    if (!parsed) {
        fprintf(stderr, "Couldn't allocate string for token: [%s]\n", token);
        exit(1);
    }

    const char *s0 = token + 1; // skip initial '"'
    char *s1 = parsed;
    char c;
    while ((c = *s0++) != '"') {
        if (c == '\\') {
            *s1++ = *s0++;
        } else *s1++ = c;
    }
    *s1 = '\0';
    return parsed;
}

static bool is_ascii_letter(char c) {
    return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z';
}

static const char *parse_name(const char *token) {

    // First, validate that the token looks like a name
    char first_c = *token;
    if (first_c == '\0') {
        fprintf(stderr, "Expected name, got empty token!\n");
        exit(1);
    } else if (first_c != '_' && !is_ascii_letter(first_c)) {
        fprintf(stderr, "Expected name, got: [%s]\n", token);
        exit(1);
    } else {
        const char *s = token;
        char c;
        while (c = *s++) {
            if (c != '_' && !is_ascii_letter(c) && !(c >= '0' && c <= '9')) {
                fprintf(stderr, "Expected name, got: [%s]\n", token);
                exit(1);
            }
        }
    }

    const char *parsed = strdup(token);
    if (!parsed) {
        fprintf(stderr, "Couldn't allocate name for token: [%s]\n", token);
        exit(1);
    }
    return parsed;
}

void compiler_compile(compiler_t *compiler, char *text) {
    // Get current frame, or add one
    compiler_frame_t *frame = compiler->frame < compiler->frames?
        compiler_push_frame(compiler): compiler->frame;
    code_t *code = frame->code;

    vm_t *vm = compiler->vm;

    char *token;
    int token_len;
    while (token = get_token(text, &token_len)) {
        text = token + token_len;

        // Save the next char immediately following the token within text,
        // and replace it with '\0', so we can pass token into functions which
        // expect it to be NUL-terminated.
        // We will put back the char before leaving this loop iteration.
        char next_c = *text;
        *text = '\0';

        if (compiler->debug_print_tokens) {
            fprintf(stderr, "GOT TOKEN: [%s]\n", token);
        }

        char first_c = token[0];
        if (first_c >= '0' && first_c <= '9') {
            // int literal
            int i = first_c - '0';
            for (int j = 1; j < token_len; j++) {
                char c = token[j];
                if (c < '0' || c > '9') {
                    fprintf(stderr, "Integer literal contains non-digit at position %i: [%s]\n", j, token);
                    exit(1);
                }
                i = i * 10 + c;
            }
            code_push_instruction(code, INSTR_LOAD_INT);
            code_push_i(code, i);
        } else if (first_c == '"') {
            // str literal
            if (token_len < 2 || token[token_len - 1] != '"') {
                fprintf(stderr, "Unterminated string literal: [%s]\n", token);
                exit(1);
            }
            const char *s = parse_string_literal(token, token_len);
            int i = vm_get_cached_str_i(vm, s);
            code_push_instruction(code, INSTR_LOAD_STR);
            code_push_i(code, i);
        } else if (first_c == '.') {
            // getter
            const char *s = parse_name(token + 1);
            int i = vm_get_cached_str_i(vm, s);
            code_push_instruction(code, INSTR_GETTER);
            code_push_i(code, i);
        } else if (first_c == '=' && token[1] == '.') {
            // setter
            const char *s = parse_name(token + 2);
            int i = vm_get_cached_str_i(vm, s);
            code_push_instruction(code, INSTR_SETTER);
            code_push_i(code, i);
        } else if (!strcmp(token, "~")) {
            code_push_instruction(code, INSTR_NEG);
        } else if (!strcmp(token, "+")) {
            code_push_instruction(code, INSTR_ADD);
        } else if (!strcmp(token, "-")) {
            code_push_instruction(code, INSTR_SUB);
        } else if (!strcmp(token, "*")) {
            code_push_instruction(code, INSTR_MUL);
        } else if (!strcmp(token, "/")) {
            code_push_instruction(code, INSTR_DIV);
        } else if (!strcmp(token, "%")) {
            code_push_instruction(code, INSTR_MOD);
        } else if (!strcmp(token, "==")) {
            code_push_instruction(code, INSTR_EQ);
        } else if (!strcmp(token, "!=")) {
            code_push_instruction(code, INSTR_NE);
        } else if (!strcmp(token, "<")) {
            code_push_instruction(code, INSTR_LT);
        } else if (!strcmp(token, "<=")) {
            code_push_instruction(code, INSTR_LE);
        } else if (!strcmp(token, ">")) {
            code_push_instruction(code, INSTR_GT);
        } else if (!strcmp(token, ">=")) {
            code_push_instruction(code, INSTR_GE);
        } else if (!strcmp(token, "@")) {
            code_push_instruction(code, INSTR_CALL);
        } else {
            // name
            const char *s = parse_name(token);
            int i = vm_get_cached_str_i(vm, s);
            code_push_instruction(code, INSTR_LOAD_GLOBAL);
            code_push_i(code, i);
        }

        // Put back the character we temporarily replaced with '\0'
        *text = next_c;
    }
}

code_t *compiler_pop_runnable_code(compiler_t *compiler) {
    if (compiler->frame == compiler->frames) {
        return (compiler->frame--)->code;
    } else return NULL;
}
