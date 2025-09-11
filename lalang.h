#ifndef _LALANG_H_
#define _LALANG_H_

#include <stdbool.h>


/************************************
* STRUCT / UNION / ENUM TYPEDEFS
************************************/

typedef enum instruction instruction_t;
typedef enum cmp_result cmp_result_t;
typedef struct type type_t;
typedef struct object object_t;
typedef struct list list_t;
typedef struct dict_item dict_item_t;
typedef struct dict dict_t;
typedef enum iteration iteration_t;
typedef union iterator_data iterator_data_t;
typedef struct custom_iterator custom_iterator_t;
typedef struct iterator iterator_t;
typedef union bytecode bytecode_t;
typedef struct code code_t;
typedef struct func func_t;
typedef struct cls cls_t;
typedef struct vm vm_t;
typedef struct compiler_frame compiler_frame_t;
typedef struct compiler compiler_t;


/**********************
* FUNCTION TYPEDEFS
**********************/

// C code which runs "on a VM", e.g. builtin functions
typedef void c_code_t(vm_t *vm);

// coercions from objects to C types
typedef bool to_bool_t(object_t *self);
typedef int to_int_t(object_t *self);
typedef const char *to_str_t(object_t *self);
typedef cmp_result_t cmp_t(object_t *self, object_t *other, vm_t *vm);

// object attributes/methods
typedef bool getter_t(object_t *self, const char *name, vm_t *vm);
typedef void print_t(object_t *self);


/****************
* MISC
****************/

enum cmp_result {
    CMP_EQ,
    CMP_NE,
    CMP_LT,
    CMP_GT
};

void print_tabs(int depth, FILE *file);
void print_string_quoted(const char *s);
char *read_file(const char *filename, bool required);
int get_index(int i, int len, const char *type_name);

#define MAX(_x, _y) ((_x) > (_y)? (_x): (_y))
#define MIN(_x, _y) ((_x) < (_y)? (_x): (_y))


/****************
* CODE
****************/

enum instruction {
    INSTR_LOAD_INT,
    INSTR_LOAD_STR,
    INSTR_LOAD_FUNC,
    // NOTE: the order of the GLOBAL and LOCAL instructions is important!
    INSTR_LOAD_GLOBAL,
    INSTR_STORE_GLOBAL,
    INSTR_CALL_GLOBAL,
    INSTR_LOAD_LOCAL,
    INSTR_STORE_LOCAL,
    INSTR_CALL_LOCAL,
    INSTR_GETTER,
    INSTR_SETTER,
    INSTR_RENAME_FUNC,

    // OPS
    // NOTE: the order of these is important!
    // They come at the end of the enum, so that we can define N_OPS in
    // terms of FIRST_OP_INSTR and N_INSTRS.
    // And the order of the operators must of course match that of
    // operator_tokens.
    INSTR_NEG,
    INSTR_ADD,
    INSTR_SUB,
    INSTR_MUL,
    INSTR_DIV,
    INSTR_MOD,
    INSTR_NOT,
    INSTR_AND,
    INSTR_OR,
    INSTR_XOR,
    INSTR_EQ,
    INSTR_NE,
    INSTR_LT,
    INSTR_LE,
    INSTR_GT,
    INSTR_GE,
    INSTR_COMMA,
    INSTR_CALL,

    N_INSTRS
};

#define FIRST_GLOBAL_INSTR INSTR_LOAD_GLOBAL
#define LAST_GLOBAL_INSTR INSTR_CALL_GLOBAL
#define FIRST_LOCAL_INSTR INSTR_LOAD_LOCAL
#define LAST_LOCAL_INSTR INSTR_CALL_LOCAL
#define N_GLOBAL_INSTRS (LAST_GLOBAL_INSTR - FIRST_GLOBAL_INSTR + 1)
#define FIRST_OP_INSTR INSTR_NEG
#define FIRST_INT_OP (INSTR_NEG - FIRST_OP_INSTR)
#define LAST_INT_OP (INSTR_MOD - FIRST_OP_INSTR)
#define FIRST_BOOL_OP (INSTR_NOT - FIRST_OP_INSTR)
#define LAST_BOOL_OP (INSTR_XOR - FIRST_OP_INSTR)
#define FIRST_CMP_OP (INSTR_EQ - FIRST_OP_INSTR)
#define LAST_CMP_OP (INSTR_GE - FIRST_OP_INSTR)
#define N_OPS (N_INSTRS - FIRST_OP_INSTR)

extern int op_arities[N_OPS];

extern const char *instruction_names[N_INSTRS];
extern const char *operator_tokens[N_OPS];

int instruction_args(instruction_t instruction);

union bytecode {
    instruction_t instruction;
    int i;
};

struct code {
    // Where this code was compiled from
    const char *filename;
    int row;
    int col;

    bool is_func; // are we a function [...] or a code block {...}?

    int len;
    bytecode_t *bytecodes;
};

code_t *code_create(const char *filename, int row, int col, bool is_func);
code_t *code_push_instruction(code_t *code, instruction_t instruction);
code_t *code_push_i(code_t *code, int i);


/****************
* TYPE
****************/

struct type {
    const char *name;
    void *data;

    // coercions to C types
    to_bool_t *to_bool;
    to_int_t *to_int;
    to_str_t *to_str;
    cmp_t *cmp;

    // type attributes/methods (used by type_type)
    getter_t *type_getter;
    getter_t *type_setter;

    // object attributes/methods
    getter_t *getter;
    getter_t *setter;
    print_t *print;
};

object_t *object_create_type(type_t *type);

extern type_t type_type;
extern object_t static_type;


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
cmp_result_t object_cmp(object_t *self, object_t *other, vm_t *vm);
list_t *object_to_pair(object_t *self);
char object_to_char(object_t *self);
void object_getter(object_t *self, const char *name, vm_t *vm);
void object_setter(object_t *self, const char *name, vm_t *vm);
void object_print(object_t *self);


/****************
* NULL
****************/

object_t *object_create_null(void);

extern type_t null_type;
extern object_t static_null;


/****************
* BOOL
****************/

object_t *object_create_bool(bool b);

extern type_t bool_type;
extern object_t static_true;
extern object_t static_false;


/****************
* INT
****************/

int int_op(int op, int i, int j);
object_t *object_create_int(int i);

extern type_t int_type;


/****************
* STR
****************/

object_t *object_create_str(const char *s);

extern type_t str_type;


/****************
* LIST
****************/

struct list {
    int len;
    object_t **elems;
};

list_t *list_create(void);
list_t *list_copy(list_t *list);
void list_grow(list_t *list, int new_len);
void list_extend(list_t *list, list_t *other);
void list_sort(list_t *list, vm_t *vm);
void list_reverse(list_t *list);
void list_assert_pair(list_t *list);
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

dict_t *dict_create(void);
dict_t *dict_copy(dict_t *dict);
object_t *object_create_dict(dict_t *dict);
dict_item_t *dict_get_item(dict_t *dict, const char *name);
object_t *dict_get(dict_t *dict, const char *name);
void dict_set(dict_t *dict, const char *name, object_t *value);
void dict_update(dict_t *dict, dict_t *other);

extern type_t dict_type;


/****************
* ITERATOR
****************/

enum iteration {
    ITER_RANGE,
    ITER_STR,
    ITER_LIST,
    // NOTE: the order of these DICT ones is important!..
    ITER_DICT_KEYS,
    ITER_DICT_VALUES,
    ITER_DICT_ITEMS,
    ITER_CUSTOM,
    N_ITERS
};

#define FIRST_DICT_ITER ITER_DICT_KEYS
#define LAST_DICT_ITER ITER_DICT_ITEMS

struct custom_iterator {
    object_t *(*next)(iterator_t *it, vm_t *vm);
    void *data;
};

union iterator_data {
    int range_start;
    list_t *list;
    dict_t *dict;
    const char *str;
    custom_iterator_t custom;
};

struct iterator {
    iteration_t iteration;
    int i;
    int end;
    iterator_data_t data;
};

extern const char *iteration_names[N_ITERS];

const char *get_iteration_name(iteration_t iteration);
iterator_t *iterator_create_slice(iteration_t iteration, int len, iterator_data_t data,
    int start, int end);
iterator_t *iterator_create(iteration_t iteration, int len, iterator_data_t data);
object_t *object_create_iterator(iterator_t *it);
object_t *object_next(object_t *obj, vm_t *vm);

extern type_t iterator_type;


/****************
* FUNC
****************/

struct func {
    const char *name;
    bool is_c_code; // uses u.c_code instead of u.code
    union {
        c_code_t *c_code;
        code_t *code;
    } u;
    list_t *stack;
    dict_t *locals;
};

func_t *func_create(const char *name);
func_t *func_copy(func_t *func);
func_t *func_create_with_c_code(const char *name, c_code_t *c_code);
func_t *func_create_with_code(const char *name, code_t *code);
object_t *object_create_func(func_t *func);

extern type_t func_type;


/********************
* CLASS & INSTANCE
********************/

struct cls {
    vm_t *vm;
    type_t *type;

    dict_t *class_attrs;

    // similar to Python's classmethods
    dict_t *class_getters;
    dict_t *class_setters;

    // similar to Python's methods/properties
    dict_t *getters;
    dict_t *setters;
};

type_t *type_create_cls(const char *name, cls_t *cls);
object_t *object_create_cls(const char *name, vm_t *vm);
object_t *object_copy_cls(cls_t *target_cls, const char *name);


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
    object_t *int_cache[VM_INT_CACHE_SIZE];
    dict_t *str_cache;
    object_t *char_cache[256];
    list_t *code_cache;
    dict_t *globals;
    dict_t *locals; // may be NULL

    int eval_depth;

    int debug_print_tokens;
    int debug_print_code;
    int debug_print_stack;
    int debug_print_eval;
};

object_t *object_create_vm(vm_t *vm);

extern type_t vm_type;

int vm_get_size(vm_t *vm);
object_t *vm_get(vm_t *vm, int i);
object_t *vm_pluck(vm_t *vm, int i);
void vm_set(vm_t *vm, int i, object_t *obj);
object_t *vm_top(vm_t *vm);
void vm_drop(vm_t *vm, int n);
object_t *vm_pop(vm_t *vm);
void vm_push(vm_t *vm, object_t *obj);
int vm_get_cached_str_i(vm_t *vm, const char *s);
object_t *vm_get_cached_str(vm_t *vm, const char *s);
object_t *vm_get_or_create_str(vm_t *vm, const char *s);
object_t *vm_get_char_str(vm_t *vm, char c);
object_t *vm_get_or_create_int(vm_t *vm, int i);
void vm_push_code(vm_t *vm, code_t *code);

vm_t *vm_create(void);
void vm_print_stack(vm_t *vm);
void vm_print_code(vm_t *vm, code_t *code, int depth);
object_t *vm_iter(vm_t *vm);
void vm_eval(vm_t *vm, code_t *code, dict_t *locals);
void vm_include(vm_t *vm, const char *filename);
void vm_eval_text(vm_t *vm, char *text, const char *filename);


/****************
* COMPILER
****************/

#define COMPILER_STACK_SIZE 1024

struct compiler_frame {
    code_t *code;
    int n_locals;
    int *locals; // indexes into vm->str_cache indicating local variable names
};

struct compiler {
    vm_t *vm;
    const char *filename;
    int row;
    int col;
    compiler_frame_t frames[COMPILER_STACK_SIZE];
    compiler_frame_t *frame;
    compiler_frame_t *last_func_frame;
};

compiler_t *compiler_create(vm_t *vm, const char *filename);
int parse_operator(const char *token);
void compiler_compile(compiler_t *compiler, char *text);
code_t *compiler_pop_runnable_code(compiler_t *compiler);

#endif
