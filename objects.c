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
    printf("<type '%s'>", type->name);
}

cmp_result_t type_cmp(object_t *self, object_t *other) {
    if (other->type != &type_type) return CMP_NE;
    else return self->data.ptr == other->data.ptr? CMP_EQ: CMP_NE;
}

bool type_getter(object_t *self, const char *name, vm_t *vm) {
    type_t *type = self->data.ptr;
    if (type->type_getter) {
        return type->type_getter(self, name, vm);
    } else if (!strcmp("name", name)) {
        vm_push(vm, vm_get_or_create_str(vm, type->name));
    } else {
        fprintf(stderr, "Type '%s' has no getter '%s'\n", type->name, name);
        exit(1);
    }
    return true;
}

bool type_setter(object_t *self, const char *name, vm_t *vm) {
    type_t *type = self->data.ptr;
    if (type->type_setter) return type->type_setter(self, name, vm);
    fprintf(stderr, "Type '%s' has no setter '%s'\n", type->name, name);
    exit(1);
}

type_t type_type = {
    .name = "type",
    .print = type_print,
    .cmp = type_cmp,
    .getter = type_getter,
    .setter = type_setter,
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
    else printf("<'%s' object at %p>", type->name, self);
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

bool bool_getter(object_t *self, const char *name, vm_t *vm) {
    int op = parse_operator(name);
    if (op < FIRST_BOOL_OP || op > LAST_BOOL_OP) return false;

    instruction_t instruction = FIRST_OP_INSTR + op;
    bool is_unop = instruction == INSTR_NOT;

    bool i, j;
    if (is_unop) {
        i = self->data.i;
    } else {
        object_t *other = vm_pop(vm);
        i = object_to_bool(other);
        j = self->data.i;
    }

    switch (instruction) {
        case INSTR_NOT: i = ~i; break;
        case INSTR_AND: i &= j; break;
        case INSTR_OR: i |= j; break;
        case INSTR_XOR: i ^= j; break;
        default:
            // should never happen...
            fprintf(stderr, "Unknown instruction in bool_getter: %i\n", instruction);
            exit(1);
    }

    vm_push(vm, object_create_bool(i));
    return true;
}

type_t bool_type = {
    .name = "bool",
    .print = bool_print,
    .to_bool = bool_to_bool,
    .getter = bool_getter,
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

cmp_result_t int_cmp(object_t *self, object_t *other) {
    int i = object_to_int(other);
    int j = self->data.i;
    if (i < j) return CMP_LT;
    else if (i > j) return CMP_GT;
    else return CMP_EQ;
}

bool int_getter(object_t *self, const char *name, vm_t *vm) {
    int op = parse_operator(name);
    if (op < FIRST_INT_OP || op > LAST_BOOL_OP) return false;

    instruction_t instruction = FIRST_OP_INSTR + op;
    bool is_unop = instruction == INSTR_NEG || instruction == INSTR_NOT;

    int i, j;
    if (is_unop) {
        i = self->data.i;
    } else {
        object_t *other = vm_pop(vm);
        i = object_to_int(other);
        j = self->data.i;
    }

    switch (instruction) {
        case INSTR_NEG: i = -i; break;
        case INSTR_ADD: i += j; break;
        case INSTR_SUB: i -= j; break;
        case INSTR_MUL: i *= j; break;
        case INSTR_DIV: i /= j; break;
        case INSTR_MOD: i %= j; break;
        case INSTR_NOT: i = ~i; break;
        case INSTR_AND: i &= j; break;
        case INSTR_OR: i |= j; break;
        case INSTR_XOR: i ^= j; break;
        default:
            // should never happen...
            fprintf(stderr, "Unknown instruction in int_getter: %i\n", instruction);
            exit(1);
    }

    vm_push(vm, vm_get_or_create_int(vm, i));
    return true;
}

type_t int_type = {
    .name = "int",
    .print = int_print,
    .to_int = int_to_int,
    .cmp = int_cmp,
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

const char *str_to_str(object_t *self) {
    return self->data.ptr;
}

cmp_result_t str_cmp(object_t *self, object_t *other) {
    const char *s1 = object_to_str(other);
    const char *s2 = self->data.ptr;
    int c = strcmp(s1, s2);
    if (c < 0) return CMP_LT;
    else if (c > 0) return CMP_GT;
    else return CMP_EQ;
}

bool str_getter(object_t *self, const char *name, vm_t *vm) {
    if (!strcmp(name, "write")) {
        fputs(self->data.ptr, stdout);
    } else return false;
    return true;
}

type_t str_type = {
    .name = "str",
    .print = str_print,
    .to_str = str_to_str,
    .cmp = str_cmp,
    .getter = str_getter,
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
    if (!value) {
        fprintf(stderr, "Attempting to store NULL at index %i of a list\n", i);
        exit(1);
    }
    if (i < 0 || i >= list->len) {
        fprintf(stderr, "List access out of bounds: %i\n", i);
        exit(1);
    }
    list->elems[i] = value;
}

void list_push(list_t *list, object_t *value) {
    if (!value) {
        fprintf(stderr, "Attempting to push NULL onto a list\n");
        exit(1);
    }
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
        if (i > 0) fputs(", ", stdout);
        object_print(list->elems[i]);
    }
    putc(']', stdout);
}

bool list_type_getter(object_t *self, const char *name, vm_t *vm) {
    if (!strcmp(name, "@")) {
        int n = object_to_int(vm_pop(vm));
        if (n < 0) {
            fprintf(stderr, "Tried to build a list of negative size %i\n", n);
            exit(1);
        }
        int size = vm_get_size(vm);
        if (n > size) {
            fprintf(stderr, "Tried to build a list of size %i from a stack of size %i\n", n, size);
            exit(1);
        }
        list_t *list = list_create();
        for (int i = n - 1; i >= 0; i--) list_push(list, vm->stack_top[-i]);
        vm->stack_top -= n;
        vm_push(vm, object_create_list(list));
    } else return false;
    return true;
}

bool list_getter(object_t *self, const char *name, vm_t *vm) {
    list_t *list = self->data.ptr;
    if (!strcmp(name, "len")) {
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
    .type_getter = list_type_getter,
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
    if (!value) {
        fprintf(stderr, "Attempting to store NULL in key '%s' of a dict\n", name);
        exit(1);
    }
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
        if (i > 0) fputs(", ", stdout);
        printf("%s: ", item->name);
        object_print(item->value);
    }
    putc('}', stdout);
}

bool dict_type_getter(object_t *self, const char *name, vm_t *vm) {
    if (!strcmp(name, "@")) {
        int n = object_to_int(vm_pop(vm));
        if (n < 0) {
            fprintf(stderr, "Tried to build a dict of negative size %i\n", n);
            exit(1);
        }
        int size = vm_get_size(vm);
        if (n * 2 > size) {
            fprintf(stderr, "Tried to build a dict of size %i (requiring %i inputs) from a stack of size %i\n", n, n * 2, size);
            exit(1);
        }
        dict_t *dict = dict_create();
        for (int i = n - 1; i >= 0; i--) {
            const char *name = object_to_str(vm->stack_top[-i * 2 - 1]);
            dict_set(dict, name, vm->stack_top[-i * 2]);
        }
        vm->stack_top -= n * 2;
        vm_push(vm, object_create_dict(dict));
    } else return false;
    return true;
}

bool dict_getter(object_t *self, const char *name, vm_t *vm) {
    dict_t *dict = self->data.ptr;
    if (!strcmp(name, "len")) {
        vm_push(vm, vm_get_or_create_int(vm, dict->len));
    } else if (
        !strcmp(name, "get_key") ||
        !strcmp(name, "get_value") ||
        !strcmp(name, "get_item")
    ) {
        // hacky methods, but useful for iteration
        int i = object_to_int(vm_pop(vm));
        if (i < 0 || i >= dict->len) {
            fprintf(stderr, "Index %i out of bounds for dict of size %i\n", i, dict->len);
            exit(1);
        }
        if (name[4] == 'k') vm_push(vm, vm_get_or_create_str(vm, dict->items[i].name));
        else if (name[4] == 'v') vm_push(vm, dict->items[i].value);
        else {
            vm_push(vm, vm_get_or_create_str(vm, dict->items[i].name));
            vm_push(vm, dict->items[i].value);
        }
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
    .type_getter = dict_type_getter,
    .getter = dict_getter,
};


/****************
* FUNC
****************/

func_t *func_create_with_c_code(const char *name, c_code_t *c_code, list_t *args) {
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

func_t *func_create(const char *name, code_t *code, list_t *args) {
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

object_t *object_create_func_with_c_code(const char *name, c_code_t *c_code, list_t *args) {
    object_t *obj = object_create(&func_type);
    obj->data.ptr = func_create_with_c_code(name, c_code, args);
    return obj;
}

object_t *object_create_func(const char *name, code_t *code, list_t *args) {
    object_t *obj = object_create(&func_type);
    obj->data.ptr = func_create(name, code, args);
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


/********************
* CLASS & INSTANCE
********************/

void cls_print(object_t *self) {
    cls_t *cls = self->type->data;
    vm_t *vm = cls->vm;
    object_t *print_obj = dict_get(cls->getters, "__print__");
    if (print_obj) {
        vm_push(vm, self);
        object_getter(print_obj, "@", vm);
    } else printf("<'%s' object at %p>", self->type->name, self);
}

bool cls_type_getter(object_t *self, const char *name, vm_t *vm) {
    // NOTE: self is a class type
    type_t *type = self->data.ptr;
    cls_t *cls = type->data;
    if (!strcmp(name, "@")) {
        // instantiate a class
        object_t *obj = object_create(type);
        obj->data.ptr = dict_create(); // instance attrs, i.e. __dict__
        vm_push(vm, obj);
        object_t *init_obj = dict_get(cls->getters, "__init__");
        if (init_obj) object_getter(init_obj, "@", vm);
    } else if (!strcmp(name, "__getters__")) {
        vm_push(vm, object_create_dict(cls->getters));
    } else if (!strcmp(name, "__setters__")) {
        vm_push(vm, object_create_dict(cls->setters));
    } else if (!strcmp(name, "__class_getters__")) {
        vm_push(vm, object_create_dict(cls->class_getters));
    } else if (!strcmp(name, "__class_setters__")) {
        vm_push(vm, object_create_dict(cls->class_setters));
    } else {
        // lookup name in class attrs
        object_t *obj = dict_get(cls->class_attrs, name);
        if (obj) vm_push(vm, obj);
        else {
            // lookup name in class getters
            object_t *getter_obj = dict_get(cls->class_getters, name);
            if (getter_obj) {
                vm_push(vm, self);
                object_getter(getter_obj, "@", vm);
            } else return false;
        }
    }
    return true;
}

bool cls_type_setter(object_t *self, const char *name, vm_t *vm) {
    // NOTE: self is a class type
    type_t *type = self->data.ptr;
    cls_t *cls = type->data;

    object_t *setter_obj = dict_get(cls->class_setters, name);
    if (setter_obj) {
        // lookup name in class setters
        vm_push(vm, self);
        object_getter(setter_obj, "@", vm);
    } else {
        // update class attrs
        object_t *obj = vm_pop(vm);
        dict_set(cls->class_attrs, name, obj);
    }
    return true;
}

bool cls_getter(object_t *self, const char *name, vm_t *vm) {
    // NOTE: self is a class instance
    type_t *type = self->type;
    cls_t *cls = type->data;
    if (!strcmp(name, "__dict__")) {
        vm_push(vm, self->data.ptr);
    } else {
        // lookup name in instance attrs
        object_t *obj = dict_get(self->data.ptr, name);
        if (obj) vm_push(vm, obj);
        else {
            // lookup name in instance getters
            object_t *getter_obj = dict_get(cls->getters, name);
            if (getter_obj) {
                vm_push(vm, self);
                object_getter(getter_obj, "@", vm);
            } else {
                // lookup name in class attrs
                object_t *obj = dict_get(cls->class_attrs, name);
                if (obj) vm_push(vm, obj);
                else return false;
            }
        }
    }
    return true;
}

bool cls_setter(object_t *self, const char *name, vm_t *vm) {
    // NOTE: self is a class instance
    type_t *type = self->type;
    cls_t *cls = type->data;

    object_t *setter_obj = dict_get(cls->setters, name);
    if (setter_obj) {
        // lookup name in instance setters
        vm_push(vm, self);
        object_getter(setter_obj, "@", vm);
    } else {
        // update instance attrs
        object_t *obj = vm_pop(vm);
        dict_set(self->data.ptr, name, obj);
    }
    return true;
}

object_t *object_create_cls(const char *name, vm_t *vm) {
    cls_t *cls = calloc(1, sizeof *cls);
    if (!cls) {
        fprintf(stderr, "Failed to allocate class '%s'\n", name);
        exit(1);
    }
    cls->vm = vm;
    cls->class_attrs = dict_create();
    cls->class_getters = dict_create();
    cls->class_setters = dict_create();
    cls->getters = dict_create();
    cls->setters = dict_create();

    type_t *type = calloc(1, sizeof *type);
    if (!type) {
        fprintf(stderr, "Failed to allocate class type '%s'\n", name);
        exit(1);
    }
    type->name = name;
    type->data = cls;
    type->print = cls_print;
    type->type_getter = cls_type_getter;
    type->type_setter = cls_type_setter;
    type->getter = cls_getter;
    type->setter = cls_setter;

    return object_create_type(type);
}
