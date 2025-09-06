#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "lalang.h"


/*************************
* BUILTIN FUNCTIONS
*************************/

void builtin_globals(vm_t *vm) {
    vm_push(vm, object_create_dict(vm->globals));
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
    object_t *i_obj = vm_pop(vm);
    int i = object_to_int(i_obj);
    vm_push(vm, vm_get(vm, i));
}

void builtin_set(vm_t *vm) {
    object_t *i_obj = vm_pop(vm);
    int i = object_to_int(i_obj);
    object_t *obj = vm_pop(vm);
    vm_set(vm, i, obj);
}

void builtin_clear(vm_t *vm) {
    // clear the stack
    vm->stack_top = vm->stack - 1;
}


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

object_t *vm_get_or_create_str(vm_t *vm, const char *s) {
    // returns a cached str object, or a fresh (uncached) one
    object_t *obj = dict_get(vm->str_cache, s);
    if (obj) return obj;
    else return object_create_str(s);
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

void vm_add_builtin(vm_t *vm, const char *name, c_code_t *code) {
    dict_set(vm->globals, name, object_create_func_c_code(name, code, NULL));
}

void vm_init(vm_t *vm) {
    // initialize stack
    vm->stack_top = vm->stack - 1;

    // initialize globals
    vm->globals = dict_create();

    // initialize singleton globals
    dict_set(vm->globals, "null", &lala_null);
    dict_set(vm->globals, "true", &lala_true);
    dict_set(vm->globals, "false", &lala_false);

    // initialize type globals
    dict_set(vm->globals, "nulltype", object_create_type(&null_type));
    dict_set(vm->globals, "bool", object_create_type(&bool_type));
    dict_set(vm->globals, "int", object_create_type(&int_type));
    dict_set(vm->globals, "str", object_create_type(&str_type));
    dict_set(vm->globals, "list", object_create_type(&list_type));
    dict_set(vm->globals, "dict", object_create_type(&dict_type));
    dict_set(vm->globals, "func", object_create_type(&func_type));

    // initialize function globals
    vm_add_builtin(vm, "globals", &builtin_globals);
    vm_add_builtin(vm, "typeof", &builtin_typeof);
    vm_add_builtin(vm, "print", &builtin_print);
    vm_add_builtin(vm, "dup", &builtin_dup);
    vm_add_builtin(vm, "drop", &builtin_drop);
    vm_add_builtin(vm, "swap", &builtin_swap);
    vm_add_builtin(vm, "get", &builtin_get);
    vm_add_builtin(vm, "set", &builtin_set);
    vm_add_builtin(vm, "clear", &builtin_clear);
    vm_add_builtin(vm, "print_stack", &vm_print_stack);

    // initialize int cache
    for (int i = VM_MIN_CACHED_INT; i <= VM_MAX_CACHED_INT; i++) {
        vm->int_cache[i - VM_MIN_CACHED_INT] = object_create_int(i);
    }

    // initialize str cache (i.e. the "string pool")
    vm->str_cache = dict_create();

}

vm_t *vm_create() {
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
    } else if (
        instruction == INSTR_GETTER || instruction == INSTR_SETTER ||
        instruction == INSTR_LOAD_GLOBAL || instruction == INSTR_STORE_GLOBAL
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

void vm_eval(vm_t *vm, code_t *code) {
    for (int i = 0; i < code->len; i++) {

        if (vm->debug_print_instructions) {
            int j = i;
            vm_print_instruction(vm, code, &j);
        }

        instruction_t instruction = code->bytecodes[i].instruction;
        if (instruction == INSTR_LOAD_INT) {
            vm_push(vm, vm_get_or_create_int(vm, code->bytecodes[++i].i));
        } else if (instruction == INSTR_LOAD_STR) {
            int j = code->bytecodes[++i].i;
            vm_push(vm, vm->str_cache->items[j].value);
        } else if (instruction == INSTR_GETTER || instruction == INSTR_SETTER) {
            int j = code->bytecodes[++i].i;
            const char *name = vm->str_cache->items[j].name;
            object_t *obj = vm_pop(vm);
            if (instruction == INSTR_GETTER) object_getter(obj, name, vm);
            if (instruction == INSTR_SETTER) object_setter(obj, name, vm);
        } else if (instruction == INSTR_LOAD_GLOBAL) {
            int j = code->bytecodes[++i].i;
            const char *name = vm->str_cache->items[j].name;
            object_t *obj = dict_get(vm->globals, name);
            if (!obj) {
                fprintf(stderr, "Global not found: %s\n", name);
                exit(1);
            }
            vm_push(vm, obj);
        } else if (instruction == INSTR_STORE_GLOBAL) {
            int j = code->bytecodes[++i].i;
            const char *name = vm->str_cache->items[j].name;
            object_t *obj = vm_pop(vm);
            dict_set(vm->globals, name, obj);
        } else {
            // operator
            int op = instruction - FIRST_OP_INSTR;
            if (op >= FIRST_CMP_OP && op <= LAST_CMP_OP) {
                object_t *self = vm_pop(vm);
                object_t *other = vm_pop(vm);
                cmp_result_t cmp = object_cmp(self, other);
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
                const char *name = operator_names[op];
                object_t *obj = vm_pop(vm);
                object_getter(obj, name, vm);
            }
        }

        if (vm->debug_print_stack) {
            printf("=== STACK:");
            vm_print_stack(vm);
            printf("=== END STACK");
        }

    }
}
