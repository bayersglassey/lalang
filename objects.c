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

void object_getter(object_t *self, const char *name, vm_t *vm) {
    type_t *type = self->type;
    bool ok = type->getter? type->getter(self, name, vm): false;
    if (!ok) {
        fprintf(stderr, "Object of type '%s' has no getter '%s'\n", type->name, name);
        exit(1);
    }
}

void object_setter(object_t *self, const char *name, vm_t *vm) {
    type_t *type = self->type;
    bool ok = type->setter? type->setter(self, name, vm): false;
    if (!ok) {
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

object_t *object_create_null() {
    return &lala_null;
}

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

object_t *object_create_bool(bool b) {
    return b? &lala_true: &lala_false;
}

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
    printf("%i", self->data.i);
}

int int_to_int(object_t *self) {
    return self->data.i;
}

bool int_getter(object_t *self, const char *name, vm_t *vm) {
    int op = parse_operator(name);
    if (op < FIRST_INT_OPERATOR || op > LAST_INT_OPERATOR) return false;

    instruction_t instruction = FIRST_OPERATOR_INSTRUCTION + op;
    bool is_binop = instruction != INSTR_NEG;

    int i, j;
    if (is_binop) {
        object_t *other = vm_pop(vm);
        i = object_to_int(other);
        j = self->data.i;
    } else {
        i = self->data.i;
    }

    switch (instruction) {
        case INSTR_NEG: i = -i; break;
        case INSTR_ADD: i += j; break;
        case INSTR_SUB: i -= j; break;
        case INSTR_MUL: i *= j; break;
        case INSTR_DIV: i /= j; break;
        case INSTR_MOD: i %= j; break;
        default: /* should never happen... */ break;
    }

    vm_push(vm, vm_get_or_create_int(vm, i));
    return true;
}

type_t int_type = {
    .name = "int",
    .print = int_print,
    .to_int = int_to_int,
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
    } else if (!strcmp(name, "len")) {
        vm_push(vm, vm_get_or_create_int(vm, list->len));
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
    } else if (!strcmp(name, "has")) {
        const char *name = object_to_str(vm_pop(vm));
        object_t *obj = dict_get(dict, name);
        vm_push(vm, object_create_bool(obj));
    } else if (!strcmp(name, "get")) {
        const char *name = object_to_str(vm_pop(vm));
        object_t *obj = dict_get(dict, name);
        if (!obj) {
            fprintf(stderr, "Tried to get missing dict key '%s'\n", name);
            exit(1);
        }
        vm_push(vm, obj);
    } else if (!strcmp(name, "get_default")) {
        const char *name = object_to_str(vm_pop(vm));
        object_t *obj_default = vm_pop(vm);
        object_t *obj = dict_get(dict, name);
        vm_push(vm, obj? obj: obj_default);
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
