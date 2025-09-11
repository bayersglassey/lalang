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

cmp_result_t type_cmp(object_t *self, object_t *other, vm_t *vm) {
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

object_t static_type = {
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

cmp_result_t object_cmp(object_t *self, object_t *other, vm_t *vm) {
    type_t *type = self->type;
    if (type->cmp) return type->cmp(self, other, vm);
    else return self == other? CMP_EQ: CMP_NE;
}

list_t *object_to_pair(object_t *self) {
    if (self->type != &list_type) {
        fprintf(stderr, "Can't interpret '%s' as a pair\n", self->type->name);
        exit(1);
    }
    list_t *list = self->data.ptr;
    list_assert_pair(list);
    return list;
}

char object_to_char(object_t *self) {
    const char *s = object_to_str(self);
    int len = strlen(s);
    if (len != 1) {
        fprintf(stderr, "Cannot coerce str of size %i to char\n", len);
        exit(1);
    }
    return s[0];
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

object_t *object_create_null(void) {
    return &static_null;
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

object_t static_null = {
    .type = &null_type,
};


/****************
* BOOL
****************/

object_t *object_create_bool(bool b) {
    return b? &static_true: &static_false;
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
    bool is_unop = op_arities[op] == 1;

    bool i = self->data.i;
    bool j;
    if (!is_unop) {
        object_t *other = vm_pop(vm);
        j = object_to_bool(other);
    }

    switch (instruction) {
        case INSTR_NOT: i = !i; break;
        case INSTR_AND: i &= j; break;
        case INSTR_OR: i |= j; break;
        case INSTR_XOR: i ^= j; break;
        default:
            // should never happen...
            fprintf(stderr, "Operator not implemented for bool: %s\n", operator_tokens[op]);
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

object_t static_true = {
    .type = &bool_type,
    .data.i = 1,
};

object_t static_false = {
    .type = &bool_type,
    .data.i = 0,
};


/****************
* INT
****************/

int int_op(int op, int i, int j) {
    instruction_t instruction = FIRST_OP_INSTR + op;
    switch (instruction) {
        case INSTR_NEG: return -i;
        case INSTR_ADD: return i + j;
        case INSTR_SUB: return i - j;
        case INSTR_MUL: return i * j;
        case INSTR_DIV: return i / j;
        case INSTR_MOD: return i % j;
        case INSTR_NOT: return ~i;
        case INSTR_AND: return i & j;
        case INSTR_OR: return i | j;
        case INSTR_XOR: return i ^ j;
        default:
            fprintf(stderr, "Operator not implemented for int: %s\n", operator_tokens[op]);
            exit(1);
    }
}

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

cmp_result_t int_cmp(object_t *self, object_t *other, vm_t *vm) {
    if (other->type != &int_type) return CMP_NE;
    int i = self->data.i;
    int j = object_to_int(other);
    if (i < j) return CMP_LT;
    else if (i > j) return CMP_GT;
    else return CMP_EQ;
}

bool int_getter(object_t *self, const char *name, vm_t *vm) {
    int op;
    if (
        op = parse_operator(name),
        op >= FIRST_INT_OP && op <= LAST_BOOL_OP
    ) {
        instruction_t instruction = FIRST_OP_INSTR + op;
        bool is_unop = op_arities[op] == 1;
        int i = self->data.i;
        int j = 0;
        if (!is_unop) {
            object_t *other = vm_pop(vm);
            j = object_to_int(other);
        }
        vm_push(vm, vm_get_or_create_int(vm, int_op(op, i, j)));
    } else if (!strcmp(name, "times")) {
        iterator_t *it = iterator_create(ITER_RANGE, self->data.i,
            (iterator_data_t){ .range_start = 0 });
        vm_push(vm, object_create_iterator(it));
    } else return false;
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
    print_string_quoted(s);
}

const char *str_to_str(object_t *self) {
    return self->data.ptr;
}

cmp_result_t str_cmp(object_t *self, object_t *other, vm_t *vm) {
    if (other->type != &str_type) return CMP_NE;
    const char *s1 = self->data.ptr;
    const char *s2 = object_to_str(other);
    int c = strcmp(s1, s2);
    if (c < 0) return CMP_LT;
    else if (c > 0) return CMP_GT;
    else return CMP_EQ;
}

bool str_getter(object_t *self, const char *name, vm_t *vm) {
    const char *s = self->data.ptr;
    if (!strcmp(name, "write")) {
        fputs(s, stdout);
    } else if (!strcmp(name, "writeline")) {
        fputs(s, stdout);
        putc('\n', stdout);
    } else if (!strcmp(name, "len")) {
        int len = strlen(s);
        vm_push(vm, vm_get_or_create_int(vm, len));
    } else if (!strcmp(name, "__iter__")) {
        iterator_t *it = iterator_create(ITER_STR, strlen(s),
            (iterator_data_t){ .str = s });
        vm_push(vm, object_create_iterator(it));
    } else if (!strcmp(name, "slice")) {
        int len = strlen(s);
        object_t *end_obj = vm_pop(vm);
        int end = end_obj == &static_null? len: object_to_int(end_obj);
        int start = object_to_int(vm_pop(vm));
        iterator_t *it = iterator_create_slice(ITER_STR, len,
            (iterator_data_t){ .str = s }, start, end);
        vm_push(vm, object_create_iterator(it));
    } else if (!strcmp(name, "get")) {
        int len = strlen(s);
        int i = get_index(object_to_int(vm_pop(vm)), len, "str");
        char c = s[i];
        vm_push(vm, vm_get_char_str(vm, c));
    } else if (!strcmp(name, "has")) {
        char c = object_to_char(vm_pop(vm));
        vm_push(vm, object_create_bool(strchr(s, c)));
    } else if (!strcmp(name, "replace")) {
        int len = strlen(s);
        char c2 = object_to_char(vm_pop(vm));
        char c1 = object_to_char(vm_pop(vm));
        char *s2 = strdup(s);
        if (!s2) {
            fprintf(stderr, "Couldn't allocate string of size %i for str .replace\n", len);
            exit(1);
        }
        for (int i = 0; i < len; i++) if (s2[i] == c1) s2[i] = c2;
        vm_push(vm, vm_get_or_create_str(vm, s2));
    } else if (!strcmp(name, "+")) {
        const char *s2 = object_to_str(vm_pop(vm));
        int len = strlen(s) + strlen(s2);
        char *s3 = malloc(len + 1);
        if (!s3) {
            fprintf(stderr, "Couldn't allocate string of size %i for str '+'\n", len);
            exit(1);
        }
        strcat(strcpy(s3, s), s2);
        vm_push(vm, vm_get_or_create_str(vm, s3));
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

list_t *list_create(void) {
    list_t *list = calloc(1, sizeof *list);
    if (!list) {
        fprintf(stderr, "Failed to allocate list\n");
        exit(1);
    }
    return list;
}

list_t *list_copy(list_t *list) {
    list_t *copy = list_create();
    int len = list->len;
    object_t **elems = malloc(len * sizeof *elems);
    if (!elems) {
        fprintf(stderr, "Failed to allocate copied list elems\n");
        exit(1);
    }
    memcpy(elems, list->elems, len * sizeof *elems);
    copy->len = list->len;
    copy->elems = elems;
    return copy;
}

void list_grow(list_t *list, int new_len) {
    if (list->len >= new_len) return;
    object_t **new_elems = realloc(list->elems, new_len * sizeof *new_elems);
    if (!new_elems) {
        fprintf(stderr, "Failed to grow list elems from %i to %i\n", list->len, new_len);
        exit(1);
    }
    list->elems = new_elems;
    list->len = new_len;
}

void list_extend(list_t *list, list_t *other) {
    int old_len = list->len;
    int new_len = old_len + other->len;
    list_grow(list, new_len);
    for (int i = old_len; i < new_len; i++) list->elems[i] = other->elems[i - old_len];
}

vm_t *list_sort_vm;
int list_sort_compare(const void *p1, const void *p2) {
    object_t *obj1 = *(object_t **)p1;
    object_t *obj2 = *(object_t **)p2;
    cmp_result_t cmp = object_cmp(obj1, obj2, list_sort_vm);
    return cmp == CMP_LT? -1: cmp == CMP_GT? 1: 0;
}
void list_sort(list_t *list, vm_t *vm) {

    // groooosss, if we used qsort_r we could pass the vm as data, but
    // qsort_r is gnu only, soooo here we are
    list_sort_vm = vm;

    qsort(list->elems, list->len, sizeof *list->elems, list_sort_compare);
}

void list_reverse(list_t *list) {
    int until = list->len / 2;
    for (int i = 0; i < until; i++) {
        int j = list->len - 1 - i;
        object_t *temp = list->elems[i];
        list->elems[i] = list->elems[j];
        list->elems[j] = temp;
    }
}

void list_assert_pair(list_t *list) {
    if (list->len != 2) {
        fprintf(stderr, "List of size %i isn't a pair\n", list->len);
        exit(1);
    }
}

object_t *object_create_list(list_t *list) {
    object_t *obj = object_create(&list_type);
    obj->data.ptr = list? list: list_create();
    return obj;
}

object_t *list_get(list_t *list, int i) {
    return list->elems[get_index(i, list->len, "list")];
}

void list_set(list_t *list, int i, object_t *value) {
    if (!value) {
        fprintf(stderr, "Attempting to store NULL at index %i of a list\n", i);
        exit(1);
    }
    list->elems[get_index(i, list->len, "list")] = value;
}

void list_push(list_t *list, object_t *value) {
    if (!value) {
        fprintf(stderr, "Attempting to push NULL onto a list\n");
        exit(1);
    }
    int new_len = list->len + 1;
    list_grow(list, new_len);
    list->elems[new_len - 1] = value;
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
    if (!strcmp(name, "new")) {
        list_t *list = list_create();
        vm_push(vm, object_create_list(list));
    } else if (!strcmp(name, "@")) {
        list_t *list;
        object_t *obj = vm_top(vm);
        if (obj->type == &list_type) {
            list = list_copy(obj->data.ptr);
        } else {
            list = list_create();
            object_t *obj_it = vm_iter(vm);
            object_t *next_obj;
            while (next_obj = object_next(obj_it, vm)) list_push(list, next_obj);
        }
        vm_push(vm, object_create_list(list));
    } else if (!strcmp(name, "build")) {
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
    } else if (!strcmp(name, ",")) {
        object_t *obj = vm_pop(vm);
        list_push(list, obj);
        vm_push(vm, self);
    } else if (!strcmp(name, "__iter__")) {
        iterator_t *it = iterator_create(ITER_LIST, list->len,
            (iterator_data_t){ .list = list });
        vm_push(vm, object_create_iterator(it));
    } else if (!strcmp(name, "slice")) {
        object_t *end_obj = vm_pop(vm);
        int end = end_obj == &static_null? list->len: object_to_int(end_obj);
        int start = object_to_int(vm_pop(vm));
        iterator_t *it = iterator_create_slice(ITER_LIST, list->len,
            (iterator_data_t){ .list = list }, start, end);
        vm_push(vm, object_create_iterator(it));
    } else if (!strcmp(name, "copy")) {
        vm_push(vm, object_create_list(list_copy(list)));
    } else if (!strcmp(name, "extend")) {
        object_t *other = vm_pop(vm);
        if (other->type != &list_type) {
            // TODO: implement iterators...
            fprintf(stderr, "Attempted to extend a list with '%s' object\n", other->type->name);
            exit(1);
        }
        list_extend(list, other->data.ptr);
    } else if (!strcmp(name, "get")) {
        int i = object_to_int(vm_pop(vm));
        vm_push(vm, list_get(list, i));
    } else if (!strcmp(name, "set")) {
        int i = object_to_int(vm_pop(vm));
        object_t *value = vm_pop(vm);
        list_set(list, i, value);
    } else if (!strcmp(name, "pop")) {
        vm_push(vm, list_pop(list));
    } else if (!strcmp(name, "push")) {
        object_t *value = vm_pop(vm);
        list_push(list, value);
    } else if (!strcmp(name, "sort")) {
        list_sort(list, vm);
    } else if (!strcmp(name, "reverse")) {
        list_reverse(list);
    } else if (!strcmp(name, "unbuild")) {
        // the inverse of list .build
        for (int i = 0; i < list->len; i++) vm_push(vm, list->elems[i]);
        vm_push(vm, vm_get_or_create_int(vm, list->len));
    } else if (!strcmp(name, "unpair")) {
        // the inverse of @pair
        list_assert_pair(list);
        vm_push(vm, list->elems[0]);
        vm_push(vm, list->elems[1]);
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

dict_t *dict_create(void) {
    dict_t *dict = calloc(1, sizeof *dict);
    if (!dict) {
        fprintf(stderr, "Failed to allocate dict\n");
        exit(1);
    }
    return dict;
}

dict_t *dict_copy(dict_t *dict) {
    dict_t *copy = dict_create();
    int len = dict->len;
    dict_item_t *items = malloc(len * sizeof *items);
    if (!items) {
        fprintf(stderr, "Failed to allocate copied dict items\n");
        exit(1);
    }
    memcpy(items, dict->items, len * sizeof *items);
    copy->len = dict->len;
    copy->items = items;
    return copy;
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

void dict_update(dict_t *dict, dict_t *other) {
    for (int i = 0; i < other->len; i++) {
        dict_item_t *item = &other->items[i];
        dict_set(dict, item->name, item->value);
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
    if (!strcmp(name, "new")) {
        dict_t *dict = dict_create();
        vm_push(vm, object_create_dict(dict));
    } else if (!strcmp(name, "@")) {
        dict_t *dict;
        object_t *obj = vm_top(vm);
        if (obj->type == &dict_type) {
            dict = dict_copy(obj->data.ptr);
        } else {
            dict = dict_create();
            object_t *obj_it = vm_iter(vm);
            object_t *next_obj;
            while (next_obj = object_next(obj_it, vm)) {
                list_t *pair = object_to_pair(next_obj);
                const char *name = object_to_str(pair->elems[0]);
                dict_set(dict, name, pair->elems[1]);
            }
        }
        vm_push(vm, object_create_dict(dict));
    } else if (!strcmp(name, "build")) {
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
    } else if (!strcmp(name, ",")) {
        list_t *pair = object_to_pair(vm_pop(vm));
        const char *name = object_to_str(pair->elems[0]);
        dict_set(dict, name, pair->elems[1]);
        vm_push(vm, self);
    } else if (
        !strcmp(name, "__iter__") ||
        !strcmp(name, "keys") ||
        !strcmp(name, "values") ||
        !strcmp(name, "items")
    ) {
        iterator_t *it = iterator_create(
            name[0] == 'v'? ITER_DICT_VALUES:
                name[0] == 'i'? ITER_DICT_ITEMS:
                ITER_DICT_KEYS,
            dict->len,
            (iterator_data_t){ .dict = dict });
        vm_push(vm, object_create_iterator(it));
    } else if (!strcmp(name, "copy")) {
        vm_push(vm, object_create_dict(dict_copy(dict)));
    } else if (!strcmp(name, "update")) {
        object_t *other_obj = vm_pop(vm);
        if (other_obj->type != &dict_type) {
            fprintf(stderr, "Can't update dict with '%s' object\n", other_obj->type->name);
            exit(1);
        }
        dict_update(dict, other_obj->data.ptr);
    } else if (
        !strcmp(name, "get_key") ||
        !strcmp(name, "get_value") ||
        !strcmp(name, "get_item")
    ) {
        // hacky methods, but useful for old-school manual iteration
        // (i.e. without iterators)
        int i = object_to_int(vm_pop(vm));
        if (i < 0 || i >= dict->len) {
            fprintf(stderr, "Index %i out of bounds for dict of size %i\n", i, dict->len);
            exit(1);
        }
        if (name[4] == 'k') vm_push(vm, vm_get_or_create_str(vm, dict->items[i].name));
        else if (name[4] == 'v') vm_push(vm, dict->items[i].value);
        else {
            vm_push(vm, dict->items[i].value);
            vm_push(vm, vm_get_or_create_str(vm, dict->items[i].name));
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
* ITERATOR
****************/

const char *iteration_names[N_ITERS] = {
    "range",
    "str",
    "list",
    "dict keys",
    "dict values",
    "dict items",
    "custom",
};

const char *get_iteration_name(iteration_t iteration) {
    if (iteration < 0 || iteration >= N_ITERS) return "unknown";
    return iteration_names[iteration];
}

iterator_t *iterator_create_slice(iteration_t iteration, int len, iterator_data_t data,
    int start, int end
) {
    iterator_t *it = calloc(1, sizeof *it);
    if (!it) {
        fprintf(stderr, "Failed to allocate %s iterator\n", get_iteration_name(iteration));
        exit(1);
    }
    if (start < 0) if ((start += len) < 0) start = 0;
    if (end < 0) if ((end += len) < 0) end = 0;
    else if (end > len) end = len;
    it->iteration = iteration;
    it->i = start;
    it->end = end > len? len: end;
    it->data = data;
    return it;
}

iterator_t *iterator_create(iteration_t iteration, int len, iterator_data_t data) {
    return iterator_create_slice(iteration, len, data, 0, len);
}

object_t *object_create_iterator(iterator_t *it) {
    object_t *obj = object_create(&iterator_type);
    obj->data.ptr = it;
    return obj;
}

object_t *object_next(object_t *obj, vm_t *vm) {
    object_getter(obj, "__next__", vm);
    if (object_to_bool(vm_pop(vm))) {
        return vm_pop(vm);
    } else return NULL; // iteration finished
}

void iterator_print(object_t *self) {
    iterator_t *it = self->data.ptr;
    printf("<%s iterator at %p>", get_iteration_name(it->iteration), self);
}

bool iterator_getter(object_t *self, const char *name, vm_t *vm) {
    iterator_t *it = self->data.ptr;
    if (!strcmp(name, "__iter__")) {
        vm_push(vm, self);
    } else if (!strcmp(name, "__next__")) {
        if (it->i >= it->end) vm_push(vm, &static_false);
        else {
            iteration_t iteration = it->iteration;
            if (iteration == ITER_RANGE) {
                vm_push(vm, vm_get_or_create_int(vm, it->data.range_start + it->i));
            } else if (iteration == ITER_STR) {
                vm_push(vm, vm_get_char_str(vm, it->data.str[it->i]));
            } else if (iteration == ITER_LIST) {
                list_t *list = it->data.list;
                vm_push(vm, list->elems[it->i]);
            } else if (iteration >= FIRST_DICT_ITER && iteration <= LAST_DICT_ITER) {
                dict_t *dict = it->data.dict;
                dict_item_t *item = &dict->items[it->i];
                if (iteration == ITER_DICT_KEYS) {
                    vm_push(vm, vm_get_or_create_str(vm, item->name));
                } else if (iteration == ITER_DICT_VALUES) {
                    vm_push(vm, item->value);
                } else if (iteration == ITER_DICT_ITEMS) {
                    list_t *pair = list_create();
                    list_grow(pair, 2);
                    pair->elems[0] = vm_get_or_create_str(vm, item->name);
                    pair->elems[1] = item->value;
                    vm_push(vm, object_create_list(pair));
                } else {
                    // we should never get here...
                    fprintf(stderr, "Unknown dict iteration tag: %i\n", iteration);
                    exit(1);
                }
            } else if (iteration == ITER_CUSTOM) {
                vm_push(vm, it->data.custom.next(it, vm));
            } else {
                // we should never get here...
                fprintf(stderr, "Unknown iteration tag: %i\n", iteration);
                exit(1);
            }
            vm_push(vm, &static_true);
            it->i++;
        }
    } else return false;
    return true;
}

type_t iterator_type = {
    .name = "iterator",
    .print = iterator_print,
    .getter = iterator_getter,
};


/****************
* FUNC
****************/

func_t *func_create(const char *name) {
    func_t *func = calloc(1, sizeof *func);
    if (!func) {
        fprintf(stderr, "Failed to allocate func: %s\n", name? name: "(no name)");
        exit(1);
    }
    func->name = name;
    return func;
}

func_t *func_copy(func_t *func) {
    func_t *copy = func_create(func->name);
    *copy = *func;
    if (copy->stack) copy->stack = list_copy(copy->stack);
    if (copy->locals) copy->locals = dict_copy(copy->locals);
    return copy;
}

func_t *func_create_with_c_code(const char *name, c_code_t *c_code) {
    func_t *func = func_create(name);
    func->is_c_code = true;
    func->u.c_code = c_code;
    return func;
}

func_t *func_create_with_code(const char *name, code_t *code) {
    func_t *func = func_create(name);
    func->is_c_code = false;
    func->u.code = code;
    return func;
}

object_t *object_create_func(func_t *func) {
    object_t *obj = object_create(&func_type);
    obj->data.ptr = func;
    return obj;
}

void func_print(object_t *self) {
    func_t *func = self->data.ptr;
    const char *name = func->name? func->name: "(no name)";
    printf("<%s %s at %p>",
        func->is_c_code? "built-in function":
            func->u.code->is_func? "function": "code block",
        name, self);
}

bool func_getter(object_t *self, const char *name, vm_t *vm) {
    func_t *func = self->data.ptr;
    if (!strcmp(name, "@")) {
        if (func->is_c_code && func->locals) {
            fprintf(stderr, "Tried to call a C function (%s) with locals\n", func->name);
            exit(1);
        }
        if (func->stack) for (int i = func->stack->len - 1; i >= 0; i--) {
            vm_push(vm, func->stack->elems[i]);
        }
        if (func->is_c_code) {
            func->u.c_code(vm);
        } else {
            dict_t *locals = func->locals? dict_copy(func->locals): NULL;
            vm_eval(vm, func->u.code, locals);
        }
    } else if (!strcmp(name, "to_dict")) {
        // run the function, and return its locals as a dict...
        // kinda hacky, but super useful for metaprogramming
        if (func->is_c_code) {
            fprintf(stderr, "Tried to call a C function (%s) with locals\n", func->name);
            exit(1);
        }
        if (func->stack) for (int i = func->stack->len - 1; i >= 0; i--) {
            vm_push(vm, func->stack->elems[i]);
        }
        dict_t *locals = func->locals? dict_copy(func->locals): dict_create();
        vm_eval(vm, func->u.code, locals);
        vm_push(vm, object_create_dict(locals));
    } else if (!strcmp(name, "name")) {
        vm_push(vm, func->name? vm_get_or_create_str(vm, func->name): &static_null);
    } else if (!strcmp(name, "copy")) {
        vm_push(vm, object_create_func(func_copy(func)));
    } else if (!strcmp(name, "stack")) {
        vm_push(vm, func->stack? object_create_list(func->stack): &static_null);
    } else if (!strcmp(name, "locals")) {
        vm_push(vm, func->locals? object_create_dict(func->locals): &static_null);
    } else if (!strcmp(name, "push_stack")) {
        object_t *obj = vm_pop(vm);
        if (!func->stack) func->stack = list_create();
        list_push(func->stack, obj);
    } else if (!strcmp(name, "set_local")) {
        const char *name = object_to_str(vm_pop(vm));
        object_t *obj = vm_pop(vm);
        if (!func->locals) func->locals = dict_create();
        dict_set(func->locals, name, obj);
    } else if (!strcmp(name, "print_code")) {
        if (func->is_c_code) printf("Can't print code of built-in function!\n");
        else vm_print_code(vm, func->u.code);
    } else return false;
    return true;
}

bool func_setter(object_t *self, const char *name, vm_t *vm) {
    func_t *func = self->data.ptr;
    if (!strcmp(name, "name")) {
        const char *name = object_to_str(vm_pop(vm));
        func->name = name;
    } else if (!strcmp(name, "stack")) {
        object_t *obj = vm_pop(vm);
        if (obj == &static_null) func->stack = NULL;
        else {
            if (obj->type != &list_type) {
                fprintf(stderr, "Tried to assign '%s' object to stack of func: %s\n",
                    obj->type->name, func->name? func->name: "(no name)");
                exit(1);
            }
            func->stack = obj->data.ptr;
        }
    } else if (!strcmp(name, "locals")) {
        object_t *obj = vm_pop(vm);
        if (obj == &static_null) func->locals = NULL;
        else {
            if (obj->type != &dict_type) {
                fprintf(stderr, "Tried to assign '%s' object to locals of func: %s\n",
                    obj->type->name, func->name? func->name: "(no name)");
                exit(1);
            }
            func->locals = obj->data.ptr;
        }
    } else return false;
    return true;
}

type_t func_type = {
    .name = "func",
    .print = func_print,
    .getter = func_getter,
    .setter = func_setter,
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

cmp_result_t cls_cmp(object_t *self, object_t *other, vm_t *vm) {
    cls_t *cls = self->type->data;
    object_t *cmp_obj = dict_get(cls->getters, "__cmp__");
    if (cmp_obj) {
        vm_push(vm, self);
        vm_push(vm, other);
        object_getter(cmp_obj, "@", vm);
        object_t *result_obj = vm_pop(vm);
        if (result_obj == &static_null) return CMP_NE;
        int result_i = object_to_int(result_obj);
        return result_i < 0? CMP_LT: result_i > 0? CMP_GT: CMP_EQ;
    } else return self == other? CMP_EQ: CMP_NE;
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
    } else if (!strcmp(name, "copy")) {
        const char *name = object_to_str(vm_pop(vm));
        vm_push(vm, object_copy_cls(cls, name));
    } else if (!strcmp(name, "__dict__")) {
        vm_push(vm, object_create_dict(cls->class_attrs));
    } else if (!strcmp(name, "__getters__")) {
        vm_push(vm, object_create_dict(cls->getters));
    } else if (!strcmp(name, "__setters__")) {
        vm_push(vm, object_create_dict(cls->setters));
    } else if (!strcmp(name, "__class_getters__")) {
        vm_push(vm, object_create_dict(cls->class_getters));
    } else if (!strcmp(name, "__class_setters__")) {
        vm_push(vm, object_create_dict(cls->class_setters));
    } else if (
        !strcmp(name, "set_getter") ||
        !strcmp(name, "set_setter") ||
        !strcmp(name, "set_class_getter") ||
        !strcmp(name, "set_class_setter")
    ) {
        dict_t *dict;
        // NOTE: make sure we don't shadow the "name" variable when doing
        // these checks...
        if (name[4] == 'c') {
            if (name[10] == 's') dict = cls->class_setters;
            else dict = cls->class_getters;
        } else {
            if (name[4] == 's') dict = cls->setters;
            else dict = cls->getters;
        }

        object_t *obj = vm_pop(vm);
        object_getter(obj, "name", vm);
        const char *name = object_to_str(vm_pop(vm));
        dict_set(dict, name, obj);
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

type_t *type_create_cls(const char *name, cls_t *cls) {
    type_t *type = calloc(1, sizeof *type);
    if (!type) {
        fprintf(stderr, "Failed to allocate class type '%s'\n", name);
        exit(1);
    }
    type->name = name;
    type->data = cls;
    type->print = cls_print;
    type->cmp = cls_cmp;
    type->type_getter = cls_type_getter;
    type->type_setter = cls_type_setter;
    type->getter = cls_getter;
    type->setter = cls_setter;
    return type;
}

object_t *object_create_cls(const char *name, vm_t *vm) {
    cls_t *cls = calloc(1, sizeof *cls);
    if (!cls) {
        fprintf(stderr, "Failed to allocate class '%s'\n", name);
        exit(1);
    }
    cls->type = type_create_cls(name, cls);
    cls->vm = vm;
    cls->class_attrs = dict_create();
    cls->class_getters = dict_create();
    cls->class_setters = dict_create();
    cls->getters = dict_create();
    cls->setters = dict_create();
    return object_create_type(cls->type);
}

object_t *object_copy_cls(cls_t *target_cls, const char *name) {
    cls_t *cls = calloc(1, sizeof *cls);
    if (!cls) {
        fprintf(stderr, "Failed to allocate class '%s' (copy of '%s')\n", name, target_cls->type->name);
        exit(1);
    }
    cls->type = type_create_cls(name, cls);
    cls->vm = target_cls->vm;
    cls->class_attrs = dict_copy(target_cls->class_attrs);
    cls->class_getters = dict_copy(target_cls->class_getters);
    cls->class_setters = dict_copy(target_cls->class_setters);
    cls->getters = dict_copy(target_cls->getters);
    cls->setters = dict_copy(target_cls->setters);
    return object_create_type(cls->type);
}
