#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "lalang.h"


/****************
* CODE
****************/

const char *instruction_names[N_INSTRS] = {
    "LOAD_INT",
    "LOAD_STR",
    "LOAD_FUNC",
    "LOAD_GLOBAL",
    "STORE_GLOBAL",
    "CALL_GLOBAL",
    "LOAD_LOCAL",
    "STORE_LOCAL",
    "CALL_LOCAL",
    "GETTER",
    "SETTER",
    "RENAME_FUNC",
    "NEG",
    "ADD",
    "SUB",
    "MUL",
    "DIV",
    "MOD",
    "NOT",
    "AND",
    "OR",
    "XOR",
    "EQ",
    "NE",
    "LT",
    "LE",
    "GT",
    "GE",
    "COMMA",
    "CALL"
};

const char *operator_tokens[N_OPS] = {
    "~",
    "+",
    "-",
    "*",
    "/",
    "%",
    "!",
    "&",
    "|",
    "^",
    "==",
    "!=",
    "<",
    "<=",
    ">",
    ">=",
    ",",
    "@"
};

int op_arities[N_OPS] = {
    1, // INSTR_NEG
    2,
    2,
    2,
    2,
    2,
    1, // INSTR_NOT
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    1 // INSTR_CALL
};

int instruction_args(instruction_t instruction) {
    switch (instruction) {
        case INSTR_LOAD_INT:
        case INSTR_LOAD_STR:
        case INSTR_LOAD_FUNC:
        case INSTR_GETTER:
        case INSTR_SETTER:
        case INSTR_LOAD_GLOBAL:
        case INSTR_STORE_GLOBAL:
        case INSTR_CALL_GLOBAL:
        case INSTR_LOAD_LOCAL:
        case INSTR_STORE_LOCAL:
        case INSTR_CALL_LOCAL:
        case INSTR_RENAME_FUNC:
            return 1;
        default: return 0;
    }
}

#define CODE_SIZE 1024

code_t *code_create(bool is_func) {
    code_t *code = calloc(1, sizeof *code);
    if (!code) {
        fprintf(stderr, "Failed to allocate code\n");
        exit(1);
    }
    code->is_func = is_func;
    return code;
}

static void code_grow(code_t *code, int len) {
    // NOTE: the size of code->bytecodes is currently static, i.e. always CODE_SIZE
    // TODO: allow code to be dynamically resized :P
    if (len >= CODE_SIZE) {
        fprintf(stderr, "Can't grow code beyond max size: %i\n", CODE_SIZE);
        exit(1);
    }
    if (code->len >= len) return;
    if (!code->bytecodes) {
        bytecode_t *bytecodes = malloc(CODE_SIZE * sizeof *bytecodes);
        if (!bytecodes) {
            fprintf(stderr, "Failed to allocate bytecodes\n");
            exit(1);
        }
        code->bytecodes = bytecodes;
    }
    code->len = len;
}

code_t *code_push_instruction(code_t *code, instruction_t instruction) {
    code_grow(code, code->len + 1);
    code->bytecodes[code->len - 1].instruction = instruction;
}

code_t *code_push_i(code_t *code, int i) {
    code_grow(code, code->len + 1);
    code->bytecodes[code->len - 1].i = i;
}
