#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "lalang.h"


const char *instruction_names[N_INSTRUCTIONS] = {
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

const char *operator_names[N_OPERATORS] = {
    "~",
    "+",
    "-",
    "*",
    "/",
    "%",
    "==",
    "!=",
    "<",
    "<=",
    ">",
    ">=",
    "@"
};


/****************
* CODE
****************/

#define CODE_SIZE 1024

code_t *code_create() {
    code_t *code = calloc(1, sizeof *code);
    if (!code) {
        fprintf(stderr, "Failed to allocate code\n");
        exit(1);
    }
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
