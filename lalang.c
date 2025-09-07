#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include "lalang.h"


/*****************
* MAIN
*****************/

static bool getenv_bool(const char *name, bool default_value) {
    const char *env = getenv(name);
    if (env) return !strcmp(env, "1");
    return default_value;
}

int main(int n_args, char **args) {
    vm_t *vm = vm_create();
    compiler_t *compiler = compiler_create(vm);

    bool quiet = getenv_bool("QUIET", false);
    bool eval = getenv_bool("EVAL", true);
    compiler->debug_print_tokens = getenv_bool("PRINT_TOKENS", false);
    compiler->debug_print_code = getenv_bool("PRINT_CODE", false);
    vm->debug_print_stack = getenv_bool("PRINT_STACK", false);
    vm->debug_print_eval = getenv_bool("PRINT_EVAL", false);

    char *line = NULL;
    size_t line_size = 0;
    bool continuing_line = false;
    while (true) {
        if (eval && !quiet) fputs(continuing_line? "... ": ">>> ", stdout);
        if (getline(&line, &line_size, stdin) < 0) {
            if (errno) {
                fprintf(stderr, "Error getting line from stdin\n");
                exit(1);
            } else break; // EOF
        }
        compiler_compile(compiler, line);
        code_t *code = compiler_pop_runnable_code(compiler);
        if (eval && code && code->len) {
            vm_eval(vm, code);
            if (!quiet) vm_print_stack(vm);
        }
        continuing_line = !code;
    }
}

