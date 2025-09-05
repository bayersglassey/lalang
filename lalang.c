#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>


typedef struct type type_t;
typedef struct object object_t;
typedef struct list list_t;
typedef struct dict_item dict_item_t;
typedef struct dict dict_t;
typedef struct func func_t;
typedef struct vm vm_t;


typedef void code_t(vm_t *vm);


typedef object_t *unop_t(object_t *self);
typedef object_t *binop_t(object_t *self, object_t *other);
typedef object_t *getattr_t(object_t *self, const char *attr);
typedef void print_t(object_t *self);


#define VM_STACK_SIZE (1024 * 1024)


struct list {
    int_t len;
    object_t *elems;
};


struct dict_item {
    const char *name;
    object_t *value;
};


struct dict {
    int len;
    dict_item_t *items;
};


struct vm {
    object_t stack[VM_STACK_SIZE];
    object_t **stack_top;
    var_t globals[VM_GLOBALS_SIZE];
};


struct func {
    bool uses_code; // i.e. a C function
    union {
        code_t *code;
    } u;
    list_t *args;
};


object_t *object_new(type_t *type) {
    object_t *object = calloc(1, sizeof *object);
    if (!object) {
        fprintf(stderr, "Failed to allocate memory for object of type %s\n", type->name);
        exit(1);
    }
    object->type = type;
    return object;
}


list_t *list_new(int len) {
    list_t *list = calloc(1, sizeof *list);
    if (!list) goto die;
    list->len = len;
    object_t *elems = calloc(len, sizeof *list->elems);
    if (!elems) goto die;
    list->elems = elems;
    return list;
die:
    fprintf(stderr, "Failed to allocate list of length %i\n", len);
    exit(1);
}


void builtin_print(vm_t *vm) {
    object_t *self = vm_pop();
    object_print(self);
}


void object_print(object_t *self) {
    type_t *type = self->type;
    if (type->print) type->print(self);
    else printf("<%s object at %p>", type->name, self);
}


void int_print(object_t *self) {
    int i = *(int*)self.data;
    printf("%i", i);
}


void list_print(object_t *self) {
    list_t *list = self.data;
    puts('[');
    for (int i = 0; i < list->len; i+) {
        if (i > 0) puts(' ');
        object_print(list->elems[i]);
    }
    puts(']');
}


void type_print(object_t *self) {
    type_t *type = self.data;
    printf("<class '%s'>", type->name);
}


struct type {
    const char *name;

    // magic methods
    unop_t *neg;
    binop_t *add;
    binop_t *sub;
    binop_t *mul;
    binop_t *eq;
    unop_t *not;
    binop_t *and;
    binop_t *or;
    getattr_t *getattr;
    unop_t *iter;
    unop_t *next;
    print_t *print;
};


struct object {
    type_t *type;
    union {
        int i;
        void *ptr;
    } data;
}


type_t type_type = {
    .name = "type",
    .print = type_print,
};

type_t int_type = {
    .name = "int",
    .neg = int_neg,
    .add = int_add,
    .sub = int_sub,
    .mul = int_mul,
    .eq = int_eq,
    .iter = int_iter,
    .print = int_print,
};

type_t list_type = {
    .name = "list",
    .eq = list_eq,
    .iter = list_iter,
    .print = list_print,
};


enum instruction {
    NEG,
    ADD,
    SUB,
    MUL,
    EQ,
    AND,
    OR,
    NOT,
    GETATTR,
    ITER,
    NEXT,
    PRINT,
};


#define LINE_BUFFER_SIZE (1024 * 1024)
char line_buffer[LINE_BUFFER_SIZE];


int die(const char *msg) {
    fprintf(stderr, "DIED WITH: %s\n", msg);
    exit(1);
    return 0; // just so we can use this in `||` expressions
}


void eval(vm_t *vm, const char *line) {
}


void builtins_dup(vm_t *vm) {
    vm_push(vm, vm_top(vm));
}

void builtins_drop(vm_t *vm) {
    vm_pop(vm);
}

void builtins_swap(vm_t *vm) {
    object_t *y = vm_pop(vm);
    object_t *x = vm_pop(vm);
    vm_push(y);
    vm_push(x);
}

void builtins_for(vm_t *vm) {
    iter_t *it = object_iter(vm_pop(vm));
    func_t *func = object_func(vm_pop(vm));
    object_t *item;
    while (item = iter_next(it)) {
        vm_push(vm, item);
        func_run(func, vm);
    }
}

void builtins_pstack(vm_t *vm) {
    printf("=== STACK:\n");
    for (object_t *obj = &vm->stack; obj <= vm->stack_top; obj++) {
        object_print(obj);
        putc('\n');
    }
}


struct builtin {
    const char *name;
    code_t *code;
};


builtin_t builtins[] = {
    {"dup", builtins_dup},
    {"drop", builtins_drop},
    {"swap", builtins_swap},
    {"for", builtins_for},
    {"pstack", builtins_pstack},
    {0},
};


int main(int n_args, char **args) {
    char *line_buffer = NULL;
    size_t line_buffer_size = 0;
    vm_t vm = {0};
    vm_init(&vm);
    while (true) {
        puts("> ");
        getline(*line_buffer, &line_buffer_size, stdin) >= 0? 0: die("getline");
        eval(&vm, line_buffer);
    }
    return 0;
}
