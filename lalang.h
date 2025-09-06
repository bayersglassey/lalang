#ifndef _LALANG_H_
#define _LALANG_H_

#include <stdbool.h>


/**********************
* ENUMS
**********************/

typedef enum instruction {
    INSTR_LOAD_INT,
    INSTR_LOAD_STR,
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
    INSTR_GETTER,
    INSTR_SETTER,
    INSTR_RETURN,
} instruction_t;

typedef enum cmp_result {
    CMP_LT,
    CMP_GT,
    CMP_EQ,
    CMP_NE
} cmp_result_t;


/****************************
* STRUCT/UNION TYPEDEFS
****************************/

typedef struct type type_t;
typedef struct object object_t;
typedef struct list list_t;
typedef struct dict_item dict_item_t;
typedef struct dict dict_t;
typedef union bytecode bytecode_t;
typedef struct code code_t;
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
typedef const char *to_str_t(object_t *self);
typedef cmp_result_t cmp_t(object_t *self, object_t *other);

// object attributes/methods
typedef bool getter_t(object_t *self, const char *attr, vm_t *vm);
typedef void print_t(object_t *self);


/****************
* TYPE
****************/

struct type {
    const char *name;

    // coercions to C types
    to_bool_t *to_bool;
    to_int_t *to_int;
    to_str_t *to_str;
    cmp_t *cmp;

    // object attributes/methods
    getter_t *getter;
    getter_t *setter;
    print_t *print;
};

object_t *object_create_type(type_t *type);

extern type_t type_type;
extern object_t lala_type;


/****************
* OBJECT
****************/

struct object {
    type_t *type;
    union {
        int i;
        void *ptr;
    } data;
};

object_t *object_create(type_t *type);
bool object_to_bool(object_t *self);
int object_to_int(object_t *self);
const char *object_to_str(object_t *self);
cmp_result_t object_cmp(object_t *self, object_t *other);
bool object_getter(object_t *self, const char *attr, vm_t *vm);
bool object_setter(object_t *self, const char *attr, vm_t *vm);
void object_print(object_t *self);


/****************
* NULL
****************/

extern type_t null_type;
extern object_t lala_null;


/****************
* BOOL
****************/

extern type_t bool_type;
extern object_t lala_true;
extern object_t lala_false ;


/****************
* INT
****************/

extern type_t int_type;


/****************
* STR
****************/

extern type_t str_type;


/****************
* LIST
****************/

struct list {
    int len;
    object_t **elems;
};

list_t *list_create();
object_t *object_create_list(list_t *list);
object_t *list_get(list_t *list, int i);
void list_set(list_t *list, int i, object_t *value);
void list_push(list_t *list, object_t *value);
object_t *list_pop(list_t *list);

extern type_t list_type;


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

dict_t *dict_create();
object_t *object_create_dict(dict_t *dict);
object_t *dict_get(dict_t *dict, const char *name);
void dict_set(dict_t *dict, const char *name, object_t *value);

extern type_t dict_type;


/****************
* FUNC
****************/

union bytecode {
    instruction_t instruction;
    int i;
};

struct code {
    int len;
    bytecode_t *bytecodes;
};

struct func {
    const char *name;
    bool is_c_code; // uses u.c_code instead of u.code
    union {
        c_code_t *c_code;
        code_t *code;
    } u;
    list_t *args;
};

extern type_t func_type;


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
    dict_t *str_cache;
    object_t *int_cache[VM_INT_CACHE_SIZE];
    dict_t *globals;
};

int vm_get_size(vm_t *vm);
object_t *vm_get(vm_t *vm, int i);
void vm_set(vm_t *vm, int i, object_t *obj);
object_t *vm_top(vm_t *vm);
object_t *vm_pop(vm_t *vm);
void vm_push(vm_t *vm, object_t *obj);
object_t *vm_get_cached_str(vm_t *vm, const char *s);
object_t *vm_get_or_create_str(vm_t *vm, const char *s);
object_t *vm_get_or_create_int(vm_t *vm, int i);

vm_t *create_vm();
code_t *vm_compile(vm_t *vm, const char *line);
void vm_eval(vm_t *vm, code_t *code);

#endif
