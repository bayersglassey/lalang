#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include "lalang.h"


/*****************
* MAIN
*****************/

static int getenv_int(const char *name, int default_value) {
    const char *env = getenv(name);
    if (!env || env[0] == '\0') return default_value;
    if (env[1] != '\0') {
        fprintf(stderr, "Expected env var %s to be a single digit, but got: %s\n", name, env);
        exit(1);
    }
    return env[0] - '0';
}

int main(int n_args, char **args) {

    // parse env vars
    bool quiet = getenv_int("QUIET", false);
    bool eval = getenv_int("EVAL", true);
    bool stdlib = getenv_int("STDLIB", true);
    int print_tokens = getenv_int("PRINT_TOKENS", 0);
    int print_code = getenv_int("PRINT_CODE", 0);
    int print_stack = getenv_int("PRINT_STACK", 0);
    int print_eval = getenv_int("PRINT_EVAL", 0);

    vm_t *vm = vm_create();
    compiler_t *compiler = compiler_create(vm, "<stdin>");

    // NOTE: include stdlib *before* turning on any debug print stuff!..
    // we can debug the stdlib itself separately
    if (stdlib) vm_include(vm, "stdlib.lala");

    vm->debug_print_tokens = print_tokens;
    vm->debug_print_code = print_code;
    vm->debug_print_stack = print_stack;
    vm->debug_print_eval = print_eval;

    char *line = NULL;
    size_t line_size = 0;
    bool continuing_line = false;
    while (true) {
        if (eval && !quiet) fputs(continuing_line? "... ": ">>> ", stdout);
        if (getline(&line, &line_size, stdin) < 0) {
            if (errno) {
                fprintf(stderr, "Error getting line from stdin: ");
                perror(NULL);
                exit(1);
            } else break; // EOF
        }
        compiler_compile(compiler, line);
        code_t *code = compiler_pop_runnable_code(compiler);
        if (eval && code && code->len) {
            vm_eval(vm, code, NULL);
            if (!quiet && line[0] != ' ') vm_print_stack(vm);
        }
        continuing_line = !code;
        compiler->row++;
    }
}

