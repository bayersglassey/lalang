#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "lalang.h"


/****************
* TYPE
****************/

object_t *object_create_type(type_t *type) {
    object_t *obj = object_create(&type_type);
    obj->data.ptr = type;
    return obj;
}

void type_print(object_t *self) {
    type_t *type = self->data.ptr;
    printf("<class '%s'>", type->name);
}

cmp_result_t type_cmp(object_t *self, object_t *other) {
    if (other->type != &type_type) return CMP_NE;
    return self->data.ptr == other->data.ptr;
}

type_t type_type = {
    .name = "type",
    .print = type_print,
    .cmp = type_cmp,
};

object_t lala_type = {
    .type = &type_type,
};


/****************
* OBJECT
****************/

object_t *object_create(type_t *type) {
    object_t *object = calloc(1, sizeof *object);
    if (!object) {
        fprintf(stderr, "Failed to allocate memory for object of type '%s'\n", type->name);
        exit(1);
    }
    object->type = type;
    return object;
}

bool object_to_bool(object_t *self) {
    type_t *type = self->type;
    if (type->to_bool) return type->to_bool(self);
    else return true;
}

int object_to_int(object_t *self) {
    type_t *type = self->type;
    if (type->to_int) return type->to_int(self);
    else {
        fprintf(stderr, "Cannot coerce '%s' to int\n", type->name);
        exit(1);
    }
}

const char *object_to_str(object_t *self) {
    type_t *type = self->type;
    if (type->to_str) return type->to_str(self);
    else {
        fprintf(stderr, "Cannot coerce '%s' to str\n", type->name);
        exit(1);
    }
}

cmp_result_t object_cmp(object_t *self, object_t *other) {
    type_t *type = self->type;
    if (type->cmp) return type->cmp(self, other);
    else return self == other? CMP_EQ: CMP_NE;
}

bool object_getter(object_t *self, const char *name, vm_t *vm) {
    type_t *type = self->type;
    bool ok = type->getter? type->getter(self, name, vm): false;
    if (ok) return true;
    else {
        fprintf(stderr, "Object of type '%s' has no getter '%s'\n", type->name, name);
        exit(1);
    }
}

bool object_setter(object_t *self, const char *name, vm_t *vm) {
    type_t *type = self->type;
    bool ok = type->setter? type->setter(self, name, vm): false;
    if (ok) return true;
    else {
        fprintf(stderr, "Object of type '%s' has no setter '%s'\n", type->name, name);
        exit(1);
    }
}

void object_print(object_t *self) {
    type_t *type = self->type;
    if (type->print) type->print(self);
    else printf("<%s object at %p>", type->name, self);
}


/****************
* NULL
****************/

void null_print(object_t *self) {
    printf("null");
}

bool null_to_bool(object_t *self) {
    return false;
}

type_t null_type = {
    .name = "nulltype",
    .print = null_print,
    .to_bool = null_to_bool,
};

object_t lala_null = {
    .type = &null_type,
};


/****************
* BOOL
****************/

void bool_print(object_t *self) {
    printf(self->data.i? "true": "false");
}

bool bool_to_bool(object_t *self) {
    return self->data.i;
}

type_t bool_type = {
    .name = "bool",
    .print = bool_print,
    .to_bool = bool_to_bool,
};

object_t lala_true = {
    .type = &bool_type,
    .data.i = 1,
};

object_t lala_false = {
    .type = &bool_type,
    .data.i = 0,
};


/****************
* INT
****************/

object_t *object_create_int(int i) {
    object_t *obj = object_create(&int_type);
    obj->data.i = i;
    return obj;
}

void int_print(object_t *self) {
    int i = *(int*)self->data.ptr;
    printf("%i", i);
}

bool int_getter(object_t *self, const char *name, vm_t *vm) {
    bool is_binop = true;
    char op;
    if (!strcmp(name, "+")) op = '+';
    else if (!strcmp(name, "-")) op = '-';
    else if (!strcmp(name, "*")) op = '*';
    else if (!strcmp(name, "/")) op = '/';
    else if (!strcmp(name, "%")) op = '%';
    else if (!strcmp(name, "~")) { is_binop = false; op = '~'; }
    else return false;

    int i, j;
    if (is_binop) {
        object_t *other = vm_pop(vm);
        i = object_to_int(other);
        j = self->data.i;
    } else {
        i = self->data.i;
    }

    switch (op) {
        case '+': i += j; break;
        case '-': i -= j; break;
        case '*': i *= j; break;
        case '/': i /= j; break;
        case '%': i %= j; break;
        case '~': i = -i; break;
        default: /* should never happen... */ break;
    }

    vm_push(vm, vm_get_or_create_int(vm, i));
    return true;
}

type_t int_type = {
    .name = "int",
    .print = int_print,
    .getter = int_getter,
};


/****************
* STR
****************/

object_t *object_create_str(const char *s) {
    object_t *obj = object_create(&str_type);
    obj->data.ptr = (void *)s;
    return obj;
}

void str_print(object_t *self) {
    const char *s = self->data.ptr;
    printf("\"%s\"", s);
}

cmp_result_t str_cmp(object_t *self, object_t *other) {
    const char *s1 = self->data.ptr;
    const char *s2 = object_to_str(other);
    int c = strcmp(s1, s2);
    if (c < 0) return CMP_LT;
    else if (c > 0) return CMP_GT;
    else return CMP_EQ;
}

type_t str_type = {
    .name = "str",
    .print = str_print,
    .cmp = str_cmp,
};


/****************
* LIST
****************/

list_t *list_create() {
    list_t *list = calloc(1, sizeof *list);
    if (!list) {
        fprintf(stderr, "Failed to allocate list\n");
        exit(1);
    }
    return list;
}

object_t *object_create_list(list_t *list) {
    object_t *obj = object_create(&list_type);
    obj->data.ptr = list? list: list_create();
    return obj;
}

object_t *list_get(list_t *list, int i) {
    if (i < 0 || i >= list->len) {
        fprintf(stderr, "List access out of bounds: %i\n", i);
        exit(1);
    }
    return list->elems[i];
}

void list_set(list_t *list, int i, object_t *value) {
    if (i < 0 || i >= list->len) {
        fprintf(stderr, "List access out of bounds: %i\n", i);
        exit(1);
    }
    list->elems[i] = value;
}

void list_push(list_t *list, object_t *value) {
    int new_len = list->len + 1;
    object_t **new_elems = realloc(list->elems, new_len * sizeof *new_elems);
    if (!new_elems) {
        fprintf(stderr, "Failed to allocate list elems\n");
        exit(1);
    }
    new_elems[new_len - 1] = value;
    list->elems = new_elems;
    list->len = new_len;
}

object_t *list_pop(list_t *list) {
    if (!list->len) {
        fprintf(stderr, "Tried to pop from an empty list\n");
        exit(1);
    }
    return list->elems[list->len - 1];
}

void list_print(object_t *self) {
    list_t *list = self->data.ptr;
    putc('[', stdout);
    for (int i = 0; i < list->len; i++) {
        if (i > 0) putc(' ', stdout);
        object_print(list->elems[i]);
    }
    putc(']', stdout);
}

bool list_getter(object_t *self, const char *name, vm_t *vm) {
    list_t *list = self->data.ptr;
    if (!strcmp(name, "new")) {
        vm_push(vm, object_create_list(NULL));
    } else if (!strcmp(name, "get")) {
        object_t *i_obj = vm_pop(vm);
        int i = object_to_int(i_obj);
        vm_push(vm, list_get(list, i));
    } else if (!strcmp(name, "set")) {
        object_t *i_obj = vm_pop(vm);
        object_t *value = vm_pop(vm);
        int i = object_to_int(i_obj);
        list_set(list, i, value);
    } else if (!strcmp(name, "pop")) {
        vm_push(vm, list_pop(list));
    } else if (!strcmp(name, "push")) {
        object_t *value = vm_pop(vm);
        list_push(list, value);
    } else return false;
    return true;
}

type_t list_type = {
    .name = "list",
    .print = list_print,
    .getter = list_getter,
};


/****************
* DICT
****************/

dict_t *dict_create() {
    dict_t *dict = calloc(1, sizeof *dict);
    if (!dict) {
        fprintf(stderr, "Failed to allocate dict\n");
        exit(1);
    }
    return dict;
}

object_t *object_create_dict(dict_t *dict) {
    object_t *obj = object_create(&dict_type);
    obj->data.ptr = dict? dict: dict_create();
    return obj;
}

dict_item_t *dict_get_item(dict_t *dict, const char *name) {
    for (int i = 0; i < dict->len; i++) {
        dict_item_t *item = &dict->items[i];
        if (!strcmp(item->name, name)) return item;
    }
    return NULL;
}

object_t *dict_get(dict_t *dict, const char *name) {
    dict_item_t *item = dict_get_item(dict, name);
    return item? item->value: NULL;
}

void dict_set(dict_t *dict, const char *name, object_t *value) {
    dict_item_t *item = dict_get_item(dict, name);
    if (item) item->value = value;
    else {
        int new_len = dict->len + 1;
        dict_item_t *new_items = realloc(dict->items, new_len * sizeof *new_items);
        if (!new_items) {
            fprintf(stderr, "Failed to allocate dict items\n");
            exit(1);
        }
        new_items[new_len - 1].name = name;
        new_items[new_len - 1].value = value;
        dict->items = new_items;
        dict->len = new_len;
    }
}

void dict_print(object_t *self) {
    dict_t *dict = self->data.ptr;
    putc('{', stdout);
    for (int i = 0; i < dict->len; i++) {
        dict_item_t *item = &dict->items[i];
        if (i > 0) putc(' ', stdout);
        printf("%s: ", item->name);
        object_print(item->value);
    }
    putc('}', stdout);
}

bool dict_getter(object_t *self, const char *name, vm_t *vm) {
    dict_t *dict = self->data.ptr;
    if (!strcmp(name, "new")) {
        vm_push(vm, object_create_dict(NULL));
    } else if (!strcmp(name, "get")) {
        const char *name = object_to_str(vm_pop(vm));
        vm_push(vm, dict_get(dict, name));
    } else if (!strcmp(name, "set")) {
        const char *name = object_to_str(vm_pop(vm));
        object_t *value = vm_pop(vm);
        dict_set(dict, name, value);
    } else return false;
    return true;
}

type_t dict_type = {
    .name = "dict",
    .print = dict_print,
    .getter = dict_getter,
};


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
* FUNC
****************/

func_t *func_create_c_code(const char *name, c_code_t *c_code, list_t *args) {
    func_t *func = calloc(1, sizeof *func);
    if (!func) {
        fprintf(stderr, "Failed to allocate c_code func: %s\n", name? name: "(no name)");
        exit(1);
    }
    func->name = name;
    func->is_c_code = true;
    func->u.c_code = c_code;
    func->args = args? args: list_create();
    return func;
}

func_t *func_create_code(const char *name, code_t *code, list_t *args) {
    func_t *func = calloc(1, sizeof *func);
    if (!func) {
        fprintf(stderr, "Failed to allocate code func: %s\n", name? name: "(no name)");
        exit(1);
    }
    func->name = name;
    func->is_c_code = false;
    func->u.code = code;
    func->args = args? args: list_create();
    return func;
}

object_t *object_create_func_c_code(const char *name, c_code_t *c_code, list_t *args) {
    object_t *obj = object_create(&func_type);
    obj->data.ptr = func_create_c_code(name, c_code, args);
    return obj;
}

object_t *object_create_func_code(const char *name, code_t *code, list_t *args) {
    object_t *obj = object_create(&func_type);
    obj->data.ptr = func_create_code(name, code, args);
    return obj;
}

void func_print(object_t *self) {
    func_t *func = self->data.ptr;
    const char *name = func->name? func->name: "(no name)";
    int n_args = func->args->len;
    if (func->is_c_code) {
        printf("<built-in function %s with %i baked-in args>", name, n_args);
    } else {
        printf("<function %s at %p with %i baked-in args>", name, func->u.code, n_args);
    }
}

bool func_getter(object_t *self, const char *name, vm_t *vm) {
    func_t *func = self->data.ptr;
    if (!strcmp(name, "@")) {
        for (int i = func->args->len - 1; i >= 0; i--) {
            vm_push(vm, func->args->elems[i]);
        }
        if (func->is_c_code) {
            func->u.c_code(vm);
        } else {
            vm_eval(vm, func->u.code);
        }
        return true;
    } else return false;
}

type_t func_type = {
    .name = "func",
    .print = func_print,
    .getter = func_getter,
};


/*************************
* BUILTIN FUNCTIONS
*************************/

void builtin_print(vm_t *vm) {
    object_t *self = vm_pop(vm);
    object_print(self);
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

void builtin_pstack(vm_t *vm) {
    printf("=== STACK:\n");
    for (object_t **obj_ptr = vm->stack; obj_ptr <= vm->stack_top; obj_ptr++) {
        object_print(*obj_ptr);
        putc('\n', stdout);
    }
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
    return vm->stack_top[-1];
}

void vm_set(vm_t *vm, int i, object_t *obj) {
    int size = vm_get_size(vm);
    if (i < 0 || i >= size) {
        fprintf(stderr, "Can't set at index %i in stack of size %i\n", i, size);
        exit(1);
    }
    vm->stack_top[-1] = obj;
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
    if (vm->stack_top >= vm->stack + VM_STACK_SIZE) {
        fprintf(stderr, "Out of stack space!\n");
        exit(1);
    }
    *(vm->stack_top--) = obj;
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
    vm_add_builtin(vm, "print", &builtin_print);
    vm_add_builtin(vm, "dup", &builtin_dup);
    vm_add_builtin(vm, "drop", &builtin_drop);
    vm_add_builtin(vm, "swap", &builtin_swap);
    vm_add_builtin(vm, "get", &builtin_get);
    vm_add_builtin(vm, "set", &builtin_set);
    vm_add_builtin(vm, "pstack", &builtin_pstack);

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

void vm_print_code(vm_t *vm, code_t *code) {
    for (int i = 0; i < code->len; i++) {
        instruction_t instruction = code->bytecodes[i].instruction;
        fputs(instruction_name[instruction], stdout);
        if (instruction == INSTR_LOAD_INT) {
            printf(" %i", code->bytecodes[++i].i);
        } else if (instruction == INSTR_LOAD_STR) {
            int j = code->bytecodes[++i].i;
            printf(" \"%s\"", vm->str_cache->items[j].name);
        } else if (instruction == INSTR_GETTER || instruction == INSTR_SETTER || instruction == INSTR_LOAD_GLOBAL) {
            int j = code->bytecodes[++i].i;
            printf(" %s", vm->str_cache->items[j].name);
        }
        putc('\n', stdout);
    }
}

void vm_eval(vm_t *vm, code_t *code) {
    fprintf(stderr, "TODO\n");
    exit(1);
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

        if (compiler->debug) {
            fprintf(stderr, "COMPILER: got token [%s]\n", token);
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


/*****************
* MAIN
*****************/

static bool getenv_bool(const char *name) {
    const char *env = getenv(name);
    return env && !strcmp(env, "1");
}

int main(int n_args, char **args) {
    char *line = NULL;
    size_t line_size = 0;
    vm_t *vm = vm_create();
    compiler_t *compiler = compiler_create(vm);
    compiler->debug = getenv_bool("DEBUG_COMPILER");
    while (true) {
        fputs("> ", stdout);
        if (getline(&line, &line_size, stdin) < 0) {
            fprintf(stderr, "Error getting line from stdin\n");
            exit(1);
        }
        compiler_compile(compiler, line);
        code_t *code = compiler_pop_runnable_code(compiler);
        if (code) {
            vm_print_code(vm, code);
            //vm_eval(vm, code);
        }
    }
    return 0;
}
