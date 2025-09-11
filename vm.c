#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#ifndef _NO_DLOPEN_
#include <dlfcn.h>
#endif

#include "lalang.h"


/*************************
* BUILTIN FUNCTIONS
*************************/

void builtin_is(vm_t *vm) {
    object_t *obj1 = vm_pop(vm);
    object_t *obj2 = vm_pop(vm);
    vm_push(vm, object_create_bool(obj1 == obj2));
}

void builtin_if(vm_t *vm) {
    object_t *if_obj = vm_pop(vm);
    object_t *cond_obj = vm_pop(vm);
    if (object_to_bool(cond_obj)) {
        object_getter(if_obj, "@", vm);
    }
}

void builtin_ifelse(vm_t *vm) {
    object_t *else_obj = vm_pop(vm);
    object_t *if_obj = vm_pop(vm);
    object_t *cond_obj = vm_pop(vm);
    if (object_to_bool(cond_obj)) {
        object_getter(if_obj, "@", vm);
    } else {
        object_getter(else_obj, "@", vm);
    }
}

void builtin_while(vm_t *vm) {
    object_t *body_obj = vm_pop(vm);
    object_t *cond_func_obj = vm_pop(vm);
    while (true) {
        object_getter(cond_func_obj, "@", vm);
        object_t *cond_obj = vm_pop(vm);
        if (!object_to_bool(cond_obj)) break;
        object_getter(body_obj, "@", vm);
    }
}

void builtin_iter(vm_t *vm) {
    object_t *obj = vm_pop(vm);
    object_getter(obj, "__iter__", vm);
}

void builtin_next(vm_t *vm) {
    object_t *obj = vm_pop(vm);
    object_getter(obj, "__next__", vm);
}

void builtin_for(vm_t *vm) {
    object_t *obj_it = vm_pop(vm);
    object_t *body_obj = vm_pop(vm);
    object_getter(obj_it, "__iter__", vm);
    obj_it = vm_pop(vm);
    object_t *next_obj;
    while (next_obj = object_next(obj_it, vm)) {
        vm_push(vm, next_obj);
        object_getter(body_obj, "@", vm);
    }
}

void builtin_range(vm_t *vm) {
    int end = object_to_int(vm_pop(vm));
    int start = object_to_int(vm_pop(vm));
    iterator_t *it = iterator_create(ITER_RANGE, end - start,
        (iterator_data_t){ .range_start = start });
    vm_push(vm, object_create_iterator(it));
}

void builtin_pair(vm_t *vm) {
    object_t *obj2 = vm_pop(vm);
    object_t *obj1 = vm_pop(vm);
    list_t *list = list_create();
    list_grow(list, 2);
    list->elems[0] = obj1;
    list->elems[1] = obj2;
    vm_push(vm, object_create_list(list));
}

void builtin_globals(vm_t *vm) {
    vm_push(vm, object_create_dict(vm->globals));
}

void builtin_locals(vm_t *vm) {
    vm_push(vm, vm->locals? object_create_dict(vm->locals): &static_null);
}

void builtin_typeof(vm_t *vm) {
    object_t *self = vm_pop(vm);
    vm_push(vm, object_create_type(self->type));
}

void builtin_print(vm_t *vm) {
    object_t *self = vm_pop(vm);
    object_print(self);
    putc('\n', stdout);
}

void builtin_dup(vm_t *vm) {
    vm_push(vm, vm_top(vm));
}

void builtin_drop(vm_t *vm) {
    vm_pop(vm);
}

void builtin_swap(vm_t *vm) {
    object_t *y = vm_pop(vm);
    object_t *x = vm_pop(vm);
    vm_push(vm, y);
    vm_push(vm, x);
}

void builtin_get(vm_t *vm) {
    int i = object_to_int(vm_pop(vm));
    vm_push(vm, vm_get(vm, i));
}

void builtin_set(vm_t *vm) {
    int i = object_to_int(vm_pop(vm));
    object_t *obj = vm_pop(vm);
    vm_set(vm, i, obj);
}

void builtin_clear(vm_t *vm) {
    // clear the stack
    vm->stack_top = vm->stack - 1;
}

void builtin_readline(vm_t *vm) {
    char *line = NULL;
    size_t n = 0;
    size_t got = getline(&line, &n, stdin);
    if (!got) {
        fprintf(stderr, "Error getting line from stdin: ");
        perror(NULL);
        exit(1);
    }
    vm_push(vm, vm_get_or_create_str(vm, line));
}

void builtin_readfile(vm_t *vm) {
    const char *filename = object_to_str(vm_pop(vm));
    char *text = read_file(filename, false);
    vm_push(vm, text? vm_get_or_create_str(vm, text): &static_null);
}

void builtin_eval(vm_t *vm) {
    const char *const_text = object_to_str(vm_pop(vm));
    char *text = strdup(const_text);
    if (!text) {
        fprintf(stderr, "Couldn't duplicate text of size %i for eval\n", (int)strlen(const_text));
        exit(1);
    }
    vm_eval_text(vm, text);
}

void builtin_dlsym(vm_t *vm) {
    const char *sym_name = object_to_str(vm_pop(vm));
    const char *filename = object_to_str(vm_pop(vm));
#ifdef _NO_DLOPEN_
    fprintf(stderr, "Not compiled with dlopen!\n");
    exit(1);
#else
    void *handle = dlopen(filename, RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
    if (!handle) {
        fprintf(stderr, "Couldn't open '%s': %s\n", filename, dlerror());
        exit(1);
    }

    // https://linuxman7.org/linux/man-pages/man3/dlvsym.3.html:
    //   In unusual cases (see NOTES) the value of the symbol could
    //   actually be NULL.  Therefore, a NULL return from dlsym() need not
    //   indicate an error.  The correct way to distinguish an error from a
    //   symbol whose value is NULL is to call dlerror(3) to clear any old
    //   error conditions, then call dlsym(), and then call dlerror(3)
    //   again, saving its return value into a variable, and check whether
    //   this saved value is not NULL.
    dlerror();
    void (*sym)(vm_t *) = dlsym(handle, sym_name);
    const char *error = dlerror();
    if (error) {
        fprintf(stderr, "Couldn't find '%s' in '%s': %s\n", sym_name, filename, error);
        exit(1);
    }

    if (!sym) {
        fprintf(stderr, "Found '%s' in '%s', but it was NULL\n", sym_name, filename);
        exit(1);
    }
    dlclose(handle);
    sym(vm);
#endif
}

void builtin_error(vm_t *vm) {
    object_t *obj = vm_pop(vm);
    if (obj->type == &str_type) {
        const char *s = obj->data.ptr;
        printf("ERROR: %s\n", s);
    } else {
        printf("ERROR: <'%s' object at %p>\n", obj->type->name, obj);
    }
    exit(1);
}

void builtin_class(vm_t *vm) {
    const char *name = object_to_str(vm_pop(vm));
    vm_push(vm, object_create_cls(name, vm));
}


/****************
* VM OBJECT
****************/

object_t *object_create_vm(vm_t *vm) {
    object_t *obj = object_create(&vm_type);
    obj->data.ptr = vm;
    return obj;
}

bool vm_type_getter(object_t *self, const char *name, vm_t *vm) {
    if (!strcmp(name, "@")) {
        vm_push(vm, object_create_vm(vm));
    } else return false;
    return true;
}

bool vm_getter(object_t *self, const char *name, vm_t *vm) {
    // NOTE: our vm doesn't necessarily have to be the one making this call! :O
    vm_t *self_vm = self->data.ptr;
    if (!strcmp(name, "print_tokens")) {
        vm_push(vm, object_create_bool(self_vm->debug_print_tokens));
    } else if (!strcmp(name, "print_code")) {
        vm_push(vm, object_create_bool(self_vm->debug_print_code));
    } else if (!strcmp(name, "print_stack")) {
        vm_push(vm, object_create_bool(self_vm->debug_print_stack));
    } else if (!strcmp(name, "print_eval")) {
        vm_push(vm, object_create_bool(self_vm->debug_print_eval));
    } else return false;
    return true;
}

bool vm_setter(object_t *self, const char *name, vm_t *vm) {
    // NOTE: our vm doesn't necessarily have to be the one making this call! :O
    vm_t *self_vm = self->data.ptr;
    if (!strcmp(name, "print_tokens")) {
        self_vm->debug_print_tokens = object_to_bool(vm_pop(vm));
    } else if (!strcmp(name, "print_code")) {
        self_vm->debug_print_code = object_to_bool(vm_pop(vm));
    } else if (!strcmp(name, "print_stack")) {
        self_vm->debug_print_stack = object_to_bool(vm_pop(vm));
    } else if (!strcmp(name, "print_eval")) {
        self_vm->debug_print_eval = object_to_bool(vm_pop(vm));
    } else return false;
    return true;
}

type_t vm_type = {
    .name = "vm",
    .type_getter = vm_type_getter,
    .getter = vm_getter,
    .setter = vm_setter,
};


/****************
* VM
****************/

int vm_get_size(vm_t *vm) {
    return vm->stack_top - vm->stack + 1;
}

object_t *vm_get(vm_t *vm, int i) {
    int size = vm_get_size(vm);
    if (i < 0 || i >= size) {
        fprintf(stderr, "Can't get at index %i from stack of size %i\n", i, size);
        exit(1);
    }
    return vm->stack_top[-i];
}

object_t *vm_pluck(vm_t *vm, int i) {
    object_t *obj = vm_get(vm, i);
    if (i > 0) {
        for (object_t **p = vm->stack_top - i; p < vm->stack_top; p++) p[0] = p[1];
    }
    vm->stack_top--;
    return obj;
}

void vm_set(vm_t *vm, int i, object_t *obj) {
    int size = vm_get_size(vm);
    if (i < 0 || i >= size) {
        fprintf(stderr, "Can't set at index %i in stack of size %i\n", i, size);
        exit(1);
    }
    vm->stack_top[-i] = obj;
}

object_t *vm_top(vm_t *vm) {
    return vm_get(vm, 0);
}

void vm_drop(vm_t *vm, int n) {
    int size = vm_get_size(vm);
    if (n > size) {
        fprintf(stderr, "Tried to pop %i items from stack of size %i\n", n, size);
        exit(1);
    }
    vm->stack_top -= n;
}

object_t *vm_pop(vm_t *vm) {
    if (vm->stack_top < vm->stack) {
        fprintf(stderr, "Tried to pop fom an empty stack!\n");
        exit(1);
    }
    return *(vm->stack_top--);
}

void vm_push(vm_t *vm, object_t *obj) {
    if (vm->stack_top >= vm->stack + VM_STACK_SIZE - 1) {
        fprintf(stderr, "Out of stack space!\n");
        exit(1);
    }
    *(++vm->stack_top) = obj;
}

int vm_get_cached_str_i(vm_t *vm, const char *s) {
    // returns the index of a cached str object, creating it if necessary
    dict_t *dict = vm->str_cache;
    dict_item_t *item = dict_get_item(dict, s);
    if (item) {
        return item - dict->items;
    } else {
        object_t *obj = object_create_str(s);
        dict_set(dict, s, obj);
        return dict->len - 1;
    }
}

object_t *vm_get_cached_str(vm_t *vm, const char *s) {
    // returns a cached str object, creating it if necessary
    int i = vm_get_cached_str_i(vm, s);
    return vm->str_cache->items[i].value;
}

// Only bother caching strings of this length or less
#define MAX_CACHED_STR_LEN 16

object_t *vm_get_or_create_str(vm_t *vm, const char *s) {
    // returns a cached str object, or a fresh (uncached) one
    if (strlen(s) < MAX_CACHED_STR_LEN) {
        object_t *obj = dict_get(vm->str_cache, s);
        if (obj) return obj;
    }
    return object_create_str(s);
}

object_t *vm_get_char_str(vm_t *vm, char c) {
    return vm->char_cache[(unsigned char)c];
}

object_t *vm_get_or_create_int(vm_t *vm, int i) {
    if (i >= VM_MIN_CACHED_INT && i <= VM_MAX_CACHED_INT) {
        return vm->int_cache[i - VM_MIN_CACHED_INT];
    } else {
        object_t *obj = object_create(&int_type);
        obj->data.i = i;
        return obj;
    }
}

void vm_set_builtin(vm_t *vm, const char *name, c_code_t *c_code) {
    func_t *func = func_create_with_c_code(name, c_code);
    dict_set(vm->globals, name, object_create_func(func));
}

void vm_push_code(vm_t *vm, code_t *code) {
    func_t *func = func_create_with_code(NULL, code);
    list_push(vm->code_cache, object_create_func(func));
}

void vm_init(vm_t *vm) {
    // initialize stack
    vm->stack_top = vm->stack - 1;

    // initialize locals
    vm->locals = NULL;

    // initialize globals
    vm->globals = dict_create();
    dict_set(vm->globals, "null", &static_null);
    dict_set(vm->globals, "true", &static_true);
    dict_set(vm->globals, "false", &static_false);
    dict_set(vm->globals, "type", object_create_type(&type_type));
    dict_set(vm->globals, "nulltype", object_create_type(&null_type));
    dict_set(vm->globals, "bool", object_create_type(&bool_type));
    dict_set(vm->globals, "int", object_create_type(&int_type));
    dict_set(vm->globals, "str", object_create_type(&str_type));
    dict_set(vm->globals, "list", object_create_type(&list_type));
    dict_set(vm->globals, "dict", object_create_type(&dict_type));
    dict_set(vm->globals, "iterator", object_create_type(&iterator_type));
    dict_set(vm->globals, "func", object_create_type(&func_type));
    dict_set(vm->globals, "vm", object_create_vm(vm));

    // initialize builtins (i.e. C function globals)
    vm_set_builtin(vm, "is", &builtin_is);
    vm_set_builtin(vm, "if", &builtin_if);
    vm_set_builtin(vm, "ifelse", &builtin_ifelse);
    vm_set_builtin(vm, "while", &builtin_while);
    vm_set_builtin(vm, "iter", &builtin_iter);
    vm_set_builtin(vm, "next", &builtin_next);
    vm_set_builtin(vm, "for", &builtin_for);
    vm_set_builtin(vm, "range", &builtin_range);
    vm_set_builtin(vm, "pair", &builtin_pair);
    vm_set_builtin(vm, "globals", &builtin_globals);
    vm_set_builtin(vm, "locals", &builtin_locals);
    vm_set_builtin(vm, "typeof", &builtin_typeof);
    vm_set_builtin(vm, "print", &builtin_print);
    vm_set_builtin(vm, "dup", &builtin_dup);
    vm_set_builtin(vm, "drop", &builtin_drop);
    vm_set_builtin(vm, "swap", &builtin_swap);
    vm_set_builtin(vm, "get", &builtin_get);
    vm_set_builtin(vm, "set", &builtin_set);
    vm_set_builtin(vm, "clear", &builtin_clear);
    vm_set_builtin(vm, "print_stack", &vm_print_stack);
    vm_set_builtin(vm, "readline", &builtin_readline);
    vm_set_builtin(vm, "readfile", &builtin_readfile);
    vm_set_builtin(vm, "eval", &builtin_eval);
    vm_set_builtin(vm, "dlsym", &builtin_dlsym);
    vm_set_builtin(vm, "error", &builtin_error);
    vm_set_builtin(vm, "class", &builtin_class);

    // initialize int cache
    for (int i = VM_MIN_CACHED_INT; i <= VM_MAX_CACHED_INT; i++) {
        vm->int_cache[i - VM_MIN_CACHED_INT] = object_create_int(i);
    }

    // initialize str cache (i.e. the "string pool")
    vm->str_cache = dict_create();

    // initialize char cache
    vm->char_cache[0] = vm_get_or_create_str(vm, "");
    for (int i = 1; i < 256; i++) {
        // TODO: keep these somewhere global, no need for malloc :P
        char *s = malloc(2);
        if (!s) {
            fprintf(stderr, "Failed to allocate small string for char %i\n", i);
            exit(1);
        }
        s[0] = (char)(unsigned char)i;
        s[1] = '\0';
        vm->char_cache[i] = vm_get_or_create_str(vm, s);
    }

    // initialize code cache
    vm->code_cache = list_create();

}

vm_t *vm_create(void) {
    vm_t *vm = calloc(1, sizeof *vm);
    if (!vm) {
        fprintf(stderr, "Failed to allocate memory for VM\n");
        exit(1);
    }
    vm_init(vm);
    return vm;
}

void vm_print_stack(vm_t *vm) {
    for (object_t **obj_ptr = vm->stack; obj_ptr <= vm->stack_top; obj_ptr++) {
        object_print(*obj_ptr);
        putc('\n', stdout);
    }
}

void vm_print_instruction(vm_t *vm, code_t *code, int *i_ptr) {
    int i = *i_ptr;

    instruction_t instruction = code->bytecodes[i].instruction;
    fputs(instruction_names[instruction], stdout);
    if (instruction == INSTR_LOAD_INT) {
        printf(" %i", code->bytecodes[++i].i);
    } else if (instruction == INSTR_LOAD_STR) {
        int j = code->bytecodes[++i].i;
        printf(" \"%s\"", vm->str_cache->items[j].name);
    } else if (instruction == INSTR_LOAD_FUNC) {
        printf(" %i", code->bytecodes[++i].i);
    } else if (
        instruction == INSTR_GETTER || instruction == INSTR_SETTER ||
        instruction >= FIRST_GLOBAL_INSTR && instruction <= LAST_LOCAL_INSTR
    ) {
        int j = code->bytecodes[++i].i;
        printf(" %s", vm->str_cache->items[j].name);
    }
    putc('\n', stdout);

    *i_ptr = i;
}

void vm_print_code(vm_t *vm, code_t *code) {
    for (int i = 0; i < code->len; i++) vm_print_instruction(vm, code, &i);
}

object_t *vm_iter(vm_t *vm) {
    object_t *obj_it = vm_pop(vm);
    object_getter(obj_it, "__iter__", vm);
    return vm_pop(vm);
}

void vm_eval(vm_t *vm, code_t *code, dict_t *locals) {

    // Set up locals
    dict_t *prev_locals;
    if (!locals && code->is_func) locals = dict_create();
    if (locals) {
        prev_locals = vm->locals;
        vm->locals = locals;
    }

    for (int i = 0; i < code->len; i++) {

        if (vm->debug_print_eval) {
            int j = i;
            vm_print_instruction(vm, code, &j);
        }

        instruction_t instruction = code->bytecodes[i].instruction;
        if (instruction == INSTR_LOAD_INT) {
            int j = code->bytecodes[++i].i;
            vm_push(vm, vm_get_or_create_int(vm, j));
        } else if (instruction == INSTR_LOAD_STR) {
            int j = code->bytecodes[++i].i;
            vm_push(vm, vm->str_cache->items[j].value);
        } else if (instruction == INSTR_LOAD_FUNC) {
            int j = code->bytecodes[++i].i;
            vm_push(vm, vm->code_cache->elems[j]);
        } else if (instruction == INSTR_GETTER || instruction == INSTR_SETTER) {
            int j = code->bytecodes[++i].i;
            const char *name = vm->str_cache->items[j].name;
            object_t *obj = vm_pop(vm);
            if (instruction == INSTR_GETTER) object_getter(obj, name, vm);
            if (instruction == INSTR_SETTER) object_setter(obj, name, vm);
        } else if (
            instruction == INSTR_LOAD_GLOBAL || instruction == INSTR_CALL_GLOBAL ||
            instruction == INSTR_LOAD_LOCAL || instruction == INSTR_CALL_LOCAL
        ) {
            int j = code->bytecodes[++i].i;
            const char *name = vm->str_cache->items[j].name;
            object_t *obj;
            bool local = instruction == INSTR_LOAD_LOCAL || instruction == INSTR_CALL_LOCAL;
            dict_t *vars = local? vm->locals: vm->globals;
            if (!vars) {
                fprintf(stderr, "Tried to store to local variable '%s', but there are no locals\n", name);
                exit(1);
            }
            obj = dict_get(vars, name);
            if (!obj) {
                fprintf(stderr, "%s variable not found: %s\n", local? "Local": "Global", name);
                exit(1);
            }
            if (instruction == INSTR_CALL_GLOBAL || instruction == INSTR_CALL_LOCAL) {
                object_getter(obj, "@", vm);
            } else {
                vm_push(vm, obj);
            }
        } else if (instruction == INSTR_RENAME_FUNC) {
            int j = code->bytecodes[++i].i;
            const char *name = vm->str_cache->items[j].name;
            object_t *obj = vm_top(vm);
            if (obj->type != &func_type) {
                fprintf(stderr, "Can't use '$' with object of type '%s'\n", obj->type->name);
                exit(1);
            }
            func_t *func = obj->data.ptr;
            func->name = name;
        } else if (instruction == INSTR_STORE_GLOBAL || instruction == INSTR_STORE_LOCAL) {
            int j = code->bytecodes[++i].i;
            const char *name = vm->str_cache->items[j].name;
            object_t *obj = vm_pop(vm);
            bool local = instruction == INSTR_STORE_LOCAL;
            dict_t *vars = local? vm->locals: vm->globals;
            if (!vars) {
                fprintf(stderr, "Tried to store to local variable '%s', but there are no locals\n", name);
                exit(1);
            }
            dict_set(vars, name, obj);
        } else {
            // operator
            int op = instruction - FIRST_OP_INSTR;
            if (op >= FIRST_CMP_OP && op <= LAST_CMP_OP) {
                object_t *other = vm_pop(vm);
                object_t *self = vm_pop(vm);
                cmp_result_t cmp = object_cmp(self, other, vm);
                bool b;
                switch (instruction) {
                    case INSTR_EQ: b = cmp == CMP_EQ; break;
                    case INSTR_NE: b = cmp != CMP_EQ; break;
                    case INSTR_LT: b = cmp == CMP_LT; break;
                    case INSTR_LE: b = cmp == CMP_LT || cmp == CMP_EQ; break;
                    case INSTR_GT: b = cmp == CMP_GT; break;
                    case INSTR_GE: b = cmp == CMP_GT || cmp == CMP_EQ; break;
                    default:
                        // should never happen...
                        fprintf(stderr, "Unknown instruction in vm_eval: %i\n", instruction);
                        exit(1);
                }
                vm_push(vm, object_create_bool(b));
            } else {
                const char *name = operator_tokens[op];
                int arity = op_arities[op];
                int n_args = arity - 1;
                // remove obj from underneath its arguments on the stack
                object_t *obj = vm_pluck(vm, n_args);
                object_getter(obj, name, vm);
            }
        }

        if (vm->debug_print_stack) {
            printf("=== STACK:");
            vm_print_stack(vm);
            printf("=== END STACK");
        }

    }

    // Restore locals
    if (locals) {
        vm->locals = prev_locals;
    }
}

void vm_include(vm_t *vm, const char *filename) {
    char *text = read_file(filename, true);
    compiler_t *compiler = compiler_create(vm);
    compiler_compile(compiler, text);
    code_t *code = compiler_pop_runnable_code(compiler);
    if (!code) {
        fprintf(stderr, "Code included from '%s' had an unterminated block\n", filename);
        exit(1);
    }
    vm_eval(vm, code, NULL);
}

void vm_eval_text(vm_t *vm, char *text) {
    compiler_t *compiler = compiler_create(vm);
    compiler_compile(compiler, text);
    code_t *code = compiler_pop_runnable_code(compiler);
    if (!code) {
        fprintf(stderr, "Code evaluated from text had an unterminated block\n");
        exit(1);
    }
    vm_eval(vm, code, NULL);
}
