#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>


/**********************
* ENUMS
**********************/

typedef enum instruction {
    INSTR_LOAD_INT,
    INSTR_LOAD_CONST,
    INSTR_LOAD_GLOBAL,
    INSTR_NEG,
    INSTR_ADD,
    INSTR_SUB,
    INSTR_MUL,
    INSTR_DIV,
    INSTR_MOD,
    INSTR_EQ,
    INSTR_NE,
    INSTR_LT,
    INSTR_LE,
    INSTR_GT,
    INSTR_GE,
    INSTR_CALL,
    INSTR_GETATTR,
    INSTR_RETURN,
} instruction_t;

typedef enum cmp_result {
    CMP_LT,
    CMP_GT,
    CMP_EQ,
    CMP_NE
} cmp_result_t;


/****************************
* STRUCT TYPEDEFS
****************************/

typedef struct type type_t;
typedef struct object object_t;
typedef struct list list_t;
typedef struct dict_item dict_item_t;
typedef struct dict dict_t;
typedef struct lala_code lala_code_t;
typedef struct func func_t;
typedef struct vm vm_t;


/**********************
* FUNCTION TYPEDEFS
**********************/

// C code which runs "on a VM", e.g. builtin functions
typedef void c_code_t(vm_t *vm);

// coercions from objects to C types
typedef bool to_bool_t(object_t *self);
typedef int to_int_t(object_t *self);
typedef cmp_t cmp_result_t(object_t *self, object_t *other);

// object attributes/methods
typedef bool getattr_t(object_t *self, const char *attr, vm_t *vm);
typedef void print_t(object_t *self);


/****************
* TYPE
****************/

struct type {
    const char *name;

    // coercions to C types
    to_bool_t *to_bool;
    to_int_t *to_int;
    cmp_t *cmp;

    // object attributes/methods
    getattr_t *getattr;
    print_t *print;
};

object_t object_create_type(type_t *type) {
    object_t *obj = object_create(type_type);
    obj->data.ptr = type;
    return obj;
}

void type_print(object_t *self) {
    type_t *type = self->data;
    printf("<class '%s'>", type->name);
}

cmp_result_t type_cmp(object_t *self, object_t *other) {
    if (other->type != type_type) return CMP_NE;
    return self->data.ptr == other.data.ptr;
}

type_t type_type = {
    .name = "type",
    .print = type_print,
    .cmp = type_cmp,
};

object_t lala_type = {
    .type = type_type,
};


/****************
* OBJECT
****************/

struct object {
    type_t *type;
    union {
        int i;
        void *ptr;
    } data;
}

object_t *object_create(type_t *type) {
    object_t *object = calloc(1, sizeof *object);
    if (!object) {
        fprintf(stderr, "Failed to allocate memory for object of type %s\n", type->name);
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

cmp_result_t object_cmp(object_t *self, object_t *other) {
    type_t *type = self->type;
    if (type->cmp) return type->cmp(self, other);
    else return self == other? CMP_EQ: CMP_NE;
}

bool object_getattr(object_t *self, const char *attr, vm_t *vm) {
    type_t *type = self->type;
    bool ok = type->getattr? type->getattr(self, attr, vm): false;
    if (ok) return true;
    else {
        fprintf(stderr, "Object of type '%s' has no attribute '%s'\n", type->name, attr);
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
    .type = null_type,
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
    .type = bool_type,
    .data.i = 1,
};

object_t lala_false = {
    .type = bool_type,
    .data.i = 0,
};


/****************
* INT
****************/

object_t object_create_int(int i) {
    object_t *obj = object_create(type_int);
    obj->data.i = i;
    return obj;
}

void int_print(object_t *self) {
    int i = *(int*)self->data;
    printf("%i", i);
}

bool int_getattr(object_t *self, const char *attr, vm_t *vm) {
    bool is_binop = true;
    char op;
    if (!strcmp(attr, "+")) op = '+';
    else if (!strcmp(attr, "-")) op = '-';
    else if (!strcmp(attr, "*")) op = '*';
    else if (!strcmp(attr, "/")) op = '/';
    else if (!strcmp(attr, "%")) op = '%';
    else if (!strcmp(attr, "~")) { is_binop = false; op = '~'; }
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

    vm_push(vm_get_or_create_int(vm, i));
    return true;
}

type_t int_type = {
    .name = "int",
    .print = int_print,
    .getattr = int_getattr,
};


/****************
* STR
****************/

object_t object_create_str(const char *s) {
    object_t *obj = object_create(type_str);
    obj->data.ptr = s;
    return obj;
}

void str_print(object_t *self) {
    const char *s = self->data;
    printf("\"%s\"", s);
}

cmp_result_t str_cmp(object_t *self, object_t *other) {
    if (other->type != str_type) return CMP_NE;
    const char *s1 = self->data.ptr;
    const char *s2 = other->data.ptr;
    int c = strcmp(s1, s2);
    if (c < 0) return CMP_LT;
    eles if (c > 0) return CMP_GT;
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

struct list {
    int_t len;
    object_t *elems;
};

list_t *list_create() {
    list_t *list = calloc(1, sizeof *list);
    if (!list) {
        fprintf(stderr, "Failed to allocate list\n");
        exit(1);
    }
    return list;
}

object_t object_create_list(list_t *list) {
    object_t *obj = object_create(type_list);
    obj->data.ptr = list? list: list_create();
    return obj;
}

void list_print(object_t *self) {
    list_t *list = self->data;
    putc('[');
    for (int i = 0; i < list->len; i+) {
        if (i > 0) putc(' ');
        object_print(list->elems[i]);
    }
    putc(']');
}

bool list_getattr(object_t *self, const char *attr, vm_t *vm) {
    if (!strcmp(attr, "new")) {
        vm_push(vm, object_create_list());
        return true;
    } else return false;
}

type_t list_type = {
    .name = "list",
    .print = list_print,
    .getattr = list_getattr,
};


/****************
* DICT
****************/

struct dict_item {
    const char *name;
    object_t *value;
};

struct dict {
    int len;
    dict_item_t *items;
};

dict_t *dict_create() {
    dict_t *dict = calloc(1, sizeof *dict);
    if (!dict) {
        fprintf(stderr, "Failed to allocate dict\n");
        exit(1);
    }
    return dict;
}

object_t object_create_dict(dict_t *dict) {
    object_t *obj = object_create(type_dict);
    obj->data.ptr = dict? dict: dict_create();
    return obj;
}

void dict_print(object_t *self) {
    dict_t *dict = self->data;
    putc('{');
    for (int i = 0; i < dict->len; i+) {
        dict_item_t *item = dict->items[i];
        if (i > 0) putc(' ');
        printf("%s: ", item->name);
        object_print(item->value);
    }
    putc('}');
}

bool dict_getattr(object_t *self, const char *attr, vm_t *vm) {
    dict_t *dict = self.data;
    if (!strcmp(attr, "new")) {
        vm_push(vm, object_create_dict());
        return true;
    } else if (!strcmp(attr, "get")) {
        const char *name = object_to_name(vm_pop(mv));
        vm_push(vm, dict_get(dict, name));
        return true;
    } else if (!strcmp(attr, "set")) {
        const char *name = object_to_name(vm_pop(mv));
        object_t *value = vm_pop(vm);
        vm_push(vm, dict_set(dict, name, value));
        return true;
    } else return false;
}

type_t dict_type = {
    .name = "dict",
    .print = dict_print,
    .getattr = dict_getattr,
};


/****************
* FUNC
****************/

union bytecode {
    instruction_t instruction;
    int i;
};

struct lala_code {
    int len;
    bytecode_t *bytecodes;
};

struct func {
    const char *name;
    bool is_c_code; // uses u.c_code instead of u.lala_code
    union {
        c_code_t *c_code;
        lala_code_t lala_code;
    } u;
    list_t *args;
};

object_t object_create_func_c_code(const char *name, c_code_t *c_code, list_t *args) {
    object_t *obj = object_create(type_func);
    obj->data.ptr = func_create_c_code(name, c_code, args);
    return obj;
}

object_t object_create_func_lala_code(const char *name, lala_code_t *lala_code, list_t *args) {
    object_t *obj = object_create(type_func);
    obj->data.ptr = func_create_lala_code(name, c_code, args);
    return obj;
}

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

func_t *func_create_lala_code(const char *name, lala_code_t *lala_code, list_t *args) {
    func_t *func = calloc(1, sizeof *func);
    if (!func) {
        fprintf(stderr, "Failed to allocate lala_code func: %s\n", name? name: "(no name)");
        exit(1);
    }
    func->name = name;
    func->is_c_code = false;
    func->u.lala_code = *lala_code;
    func->args = args? args: list_create();
    return func;
}

void func_print(object_t *self) {
    func_t *func = self->data;
    const char *name = func->name? func->name: "(no name)";
    int n_args = func->args->len;
    if (func->is_c_code) {
        printf("<built-in function %s with %i baked-in args>", name, n_args);
    } else {
        printf("<function %s at %p with %i baked-in args>", name, func->u.lala_code, n_args);
    }
}

bool func_getattr(object_t *self, const char *attr, vm_t *vm) {
    func_t *func = self->data;
    if (!strcmp(attr, "@")) {
        for (int i = func->args->len - 1; i >= 0; i--) {
            vm_push(vm, func->args->elems[i]);
        }
        if (func->is_c_code) {
            func->u.c_code(vm);
        } else {
            vm_eval(vm, func->u.lala_code);
        }
        return true;
    } else return false;
}

type_t func_type = {
    .name = "func",
    .print = func_print,
    .getattr = func_getattr,
};


/*************************
* BUILTIN FUNCTIONS
*************************/

void builtin_print(vm_t *vm) {
    object_t *self = vm_pop();
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
    vm_push(y);
    vm_push(x);
}

void builtin_get(vm_t *vm) {
    object_t *i_obj = vm_pop(vm);
    int i = object_to_int(i_obj);
    return vm_get(vm, i);
}

void builtin_set(vm_t *vm) {
    object_t *i_obj = vm_pop(vm);
    int i = object_to_int(i_obj);
    object_t *obj = vm_pop(vm);
    return vm_set(vm, i, obj);
}

void builtin_pstack(vm_t *vm) {
    printf("=== STACK:\n");
    for (object_t *obj = &vm->stack; obj <= vm->stack_top; obj++) {
        object_print(obj);
        putc('\n');
    }
}


/****************
* VM
****************/

#define VM_STACK_SIZE (1024 * 1024)

#define VM_MIN_CACHED_INT (-100)
#define VM_MAX_CACHED_INT (100)
#define VM_INT_CACHE_SIZE (VM_MAX_CACHED_INT - VM_MIN_CACHED_INT + 1)

struct vm {
    object_t *stack[VM_STACK_SIZE];
    object_t **stack_top;
    list_t *constants;
    dict_t *str_cache;
    object_t int_cache[VM_INT_CACHE_SIZE];
    dict_t *globals;
};

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

object_t *vm_get_cached_str(vm_t *vm, const char *s) {
    // returns a cached str object, creating it if necessary
    object_t *obj = dict_get(vm->str_cache, s);
    if (obj) return obj;
    obj = object_create_str(s);
    dict_set(vm->str_cache, s, obj);
    return obj;
}

object_t *vm_get_or_create_str(vm_t *vm, const char *s) {
    // returns a cached str object, or a fresh (uncached) one
    object_t *obj = dict_get(vm->str_cache, s);
    if (obj) return obj;
    else return object_create_str(s);
}

object_t vm_get_or_create_int(vm_t *vm, int i) {
    if (i >= VM_MIN_CACHED_INT && i <= VM_MAX_CACHED_INT) {
        return vm->int_cache[i - VM_MIN_CACHED_INT];
    } else {
        object_t *obj = object_create(type_int);
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

    // initialize constants
    vm->constants = list_create();

    // initialize globals
    vm->globals = dict_create();

    // initialize singleton globals
    dict_set(vm->globals, "null", lala_null);
    dict_set(vm->globals, "true", lala_true);
    dict_set(vm->globals, "false", lala_false);

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

lala_code_t *vm_compile(vm_t *vm, const char *line) {
    fprintf(stderr, "TODO\n");
    exit(1);
}

void vm_eval(vm_t *vm, lala_code_t *code) {
    fprintf(stderr, "TODO\n");
    exit(1);
}


/*****************
* MAIN
*****************/

int main(int n_args, char **args) {
    char *line = NULL;
    size_t line_size = 0;
    vm_t vm = {0};
    vm_init(&vm);
    while (true) {
        puts("> ");
        if (getline(&line, &line_size, stdin) < 0) {
            perror("getline");
            exit(1);
        }
        lala_code_t *code = vm_compile(&vm, line);
        vm_eval(&vm, code);
    }
    return 0;
}
