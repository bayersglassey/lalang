#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "lalang.h"


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

static compiler_frame_t *compiler_push_frame(compiler_t *compiler, bool is_func) {
    compiler_frame_t *frame = ++compiler->frame;
    frame->code = code_create(is_func);
    frame->n_locals = 0;
    frame->locals = NULL;
    if (is_func) compiler->last_func_frame = frame;
    return frame;
}

static instruction_t compiler_process_global_ref(
    compiler_t *compiler, instruction_t instruction, int str_cache_i
) {
    // Takes a GLOBAL instruction, plus its string cache index (i.e. the .i
    // of the following bytecode).
    // Returns the instruction, *or* the instruction converted to LOCAL,
    // depending on whether we're in a function scope and the string is known
    // to be in that function's locals...
    compiler_frame_t *last_func_frame = compiler->last_func_frame;
    if (last_func_frame) {
        for (int j = 0; j < last_func_frame->n_locals; j++) {
            // Check whether
            if (last_func_frame->locals[j] != str_cache_i) continue;
            return instruction + N_GLOBAL_INSTRS; // convert to LOCAL
        }
    }
    return instruction;
}

static compiler_frame_t *compiler_pop_frame(compiler_t *compiler) {
    if (compiler->frame < compiler->frames) {
        fprintf(stderr, "Tried to pop from an empty frame stack\n");
        exit(1);
    }
    compiler_frame_t *popped_frame = compiler->frame--;
    if (popped_frame == compiler->last_func_frame) {
        // we were the last "func frame", but now we're being popped, so find
        // the previous "func frame"
        compiler->last_func_frame = NULL;
        for (compiler_frame_t *frame = compiler->frame; frame >= compiler->frames; frame--) {
            if (frame->code->is_func) {
                compiler->last_func_frame = frame;
                break;
            }
        }
    }
    if (compiler->debug_print_code) {
        printf("=== PARSED CODE:\n");
        vm_print_code(compiler->vm, popped_frame->code);
        printf("=== END CODE\n");
    }
    free(popped_frame->locals); // currently our only use of free in this codebase... hooray?
    return popped_frame;
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
            char c = *s0++;
            *s1++ = c == 'n'? '\n': c;
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

int parse_operator(const char *token) {
    for (int op = 0; op < N_OPS; op++) {
        if (!strcmp(token, operator_tokens[op])) return op;
    }
    return -1;
}

static void compiler_frame_push_local(compiler_frame_t *frame, int cached_str_i) {
    // mark the indicated variable name as being local to frame->code
    // NOTE: if we arrive here, frame->code->is_func must be true
    for (int i = 0; i < frame->n_locals; i++) {
        if (frame->locals[i] == cached_str_i) return; // already there
    }
    int new_n_locals = frame->n_locals + 1;
    int *new_locals = realloc(frame->locals, new_n_locals * sizeof *new_locals);
    if (!new_locals) {
        fprintf(stderr, "Failed to allocate compiler frame locals\n");
        exit(1);
    }
    new_locals[new_n_locals - 1] = cached_str_i;
    frame->locals = new_locals;
    frame->n_locals = new_n_locals;
}

void compiler_compile(compiler_t *compiler, char *text) {
    // Get current frame, or add one
    compiler_frame_t *frame = compiler->frame < compiler->frames?
        compiler_push_frame(compiler, false): compiler->frame;
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
        int op;
        if (!strcmp(token, ">>>") || !strcmp(token, "...")) {
            // just ignore these, so we can copy-paste from/to the REPL!
        } else if (
            first_c >= '0' && first_c <= '9' ||
            first_c == '-' && token[1] >= '0' && token[1] <= '9'
        ) {
            // int literal
            bool neg = first_c == '-';
            int i = token[neg] - '0';
            for (int j = neg + 1; j < token_len; j++) {
                char c = token[j];
                if (c < '0' || c > '9') {
                    fprintf(stderr, "Integer literal contains non-digit at position %i: [%s]\n", j, token);
                    exit(1);
                }
                i = i * 10 + (c - '0');
            }
            code_push_instruction(code, INSTR_LOAD_INT);
            code_push_i(code, neg? -i: i);
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
        } else if ((op = parse_operator(token)) >= 0) {
            // operator
            // NOTE: need to check for this before the check for '=' followed
            // by a name
            code_push_instruction(code, FIRST_OP_INSTR + op);
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
        } else if (first_c == '=') {
            // store global/local
            const char *s = parse_name(token + 1);
            int i = vm_get_cached_str_i(vm, s);
            compiler_frame_t *last_func_frame = compiler->last_func_frame;
            if (last_func_frame) {
                compiler_frame_push_local(last_func_frame, i);
                code_push_instruction(code, INSTR_STORE_LOCAL);
            } else {
                code_push_instruction(code, INSTR_STORE_GLOBAL);
            }
            code_push_i(code, i);
        } else if (first_c == '@' && token[1] != '\0') {
            // call global/local
            const char *s = parse_name(token + 1);
            int i = vm_get_cached_str_i(vm, s);
            instruction_t instruction = compiler_process_global_ref(compiler,
                INSTR_CALL_GLOBAL, i);
            code_push_instruction(code, instruction);
            code_push_i(code, i);
        } else if (!strcmp(token, "(") || !strcmp(token, ")")) {
            // no-ops!
            // these can be used to indicate that a given code sequence is
            // expected to result in a single value being pushed onto the
            // stack.
            // if we wanted to be really fancy, we would add bytecodes for
            // these, and *assert* that the expected stack effect had taken
            // place.
        } else if (!strcmp(token, "{") || !strcmp(token, "[")) {
            // start code block
            bool is_func = token[0] == '[';
            frame = compiler_push_frame(compiler, is_func);
            code = frame->code;
        } else if (!strcmp(token, "}") || !strcmp(token, "]")) {
            // end code block
            if (compiler->frame <= compiler->frames) {
                fprintf(stderr, "Unterminated block\n");
                exit(1);
            }
            bool was_func = compiler->frame->code->is_func;
            bool is_func = token[0] == ']';
            if (was_func != is_func) {
                fprintf(stderr, "Expected '%c', got '%c'\n",
                    was_func? ']': '}',
                    is_func? ']': '}');
                exit(1);
            }
            vm_push_code(vm, code);
            int i = vm->code_cache->len - 1;
            compiler_pop_frame(compiler);
            frame = compiler->frame;
            code = frame->code;
            code_push_instruction(code, INSTR_LOAD_FUNC);
            code_push_i(code, i);
        } else {
            // load global/local
            const char *s = parse_name(token);
            int i = vm_get_cached_str_i(vm, s);
            instruction_t instruction = compiler_process_global_ref(compiler,
                INSTR_LOAD_GLOBAL, i);
            code_push_instruction(code, instruction);
            code_push_i(code, i);
        }

        // Put back the character we temporarily replaced with '\0'
        *text = next_c;
    }
}

code_t *compiler_pop_runnable_code(compiler_t *compiler) {
    if (compiler->frame == compiler->frames) {
        code_t *code = (compiler->frame--)->code;
        if (compiler->debug_print_code) {
            printf("=== PARSED RUNNABLE CODE:\n");
            vm_print_code(compiler->vm, code);
            printf("=== END CODE\n");
        }
        return code;
    } else return NULL;
}
