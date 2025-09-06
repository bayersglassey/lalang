#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "lalang.h"


/*****************
* MAIN
*****************/

static bool getenv_bool(const char *name) {
    const char *env = getenv(name);
    return env && !strcmp(env, "1");
}

int main(int n_args, char **args) {
    vm_t *vm = vm_create();
    compiler_t *compiler = compiler_create(vm);

    compiler->debug_print_tokens = getenv_bool("PRINT_TOKENS");
    bool debug_print_code = getenv_bool("PRINT_CODE");

    char *line = NULL;
    size_t line_size = 0;
    while (true) {
        fputs("> ", stdout);
        if (getline(&line, &line_size, stdin) < 0) {
            fprintf(stderr, "Error getting line from stdin\n");
            exit(1);
        }
        compiler_compile(compiler, line);
        code_t *code = compiler_pop_runnable_code(compiler);
        if (code) {
            if (debug_print_code) {
                printf("=== PARSED CODE:\n");
                vm_print_code(vm, code);
                printf("=== END CODE\n");
            }
            vm_eval(vm, code);
        }
    }
}
