#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "nlist.h"


nlist_t *nlist_create(int len) {
    nlist_t *nlist = calloc(1, sizeof *nlist);
    if (!nlist) {
        fprintf(stderr, "Failed to allocate nlist\n");
        exit(1);
    }
    int *elems = malloc(len * sizeof *elems);
    if (!elems) {
        fprintf(stderr, "Failed to allocate %i nlist elems\n", len);
        exit(1);
    }
    nlist->len = len;
    nlist->elems = elems;
    return nlist;
}

nlist_t *nlist_from_list(list_t *list) {
    nlist_t *nlist = nlist_create(list->len);
    for (int i = 0; i < list->len; i++) {
        nlist->elems[i] = object_to_int(list->elems[i]);
    }
    return nlist;
}

list_t *nlist_to_list(nlist_t *nlist, vm_t *vm) {
    list_t *list = list_create();
    list_grow(list, nlist->len);
    for (int i = 0; i < nlist->len; i++) {
        list->elems[i] = vm_get_or_create_int(vm, nlist->elems[i]);
    }
    return list;
}

nlist_t *nlist_copy(nlist_t *nlist) {
    nlist_t *copy = nlist_create(nlist->len);
    for (int i = 0; i < nlist->len; i++) {
        copy->elems[i] = nlist->elems[i];
    }
    return nlist;
}

object_t *object_create_nlist(nlist_t *nlist) {
    object_t *obj = object_create(&nlist_type);
    obj->data.ptr = nlist;
    return obj;
}

int nlist_get(nlist_t *nlist, int i) {
    if (i < 0 || i >= nlist->len) {
        fprintf(stderr, "Attempted to get at index %i of nlist of size %i\n", i, nlist->len);
        exit(1);
    }
    return nlist->elems[i];
}

void nlist_set(nlist_t *nlist, int i, int value) {
    if (i < 0 || i >= nlist->len) {
        fprintf(stderr, "Attempted to set at index %i of nlist of size %i\n", i, nlist->len);
        exit(1);
    }
    nlist->elems[i] = value;
}

void nlist_print(object_t *self) {
    nlist_t *nlist = self->data.ptr;
    fputs("nlist([", stdout);
    for (int i = 0; i < nlist->len; i++) {
        if (i > 0) fputs(", ", stdout);
        printf("%i", nlist->elems[i]);
    }
    fputs("])", stdout);
}

bool nlist_type_getter(object_t *self, const char *name, vm_t *vm) {
    if (!strcmp(name, "@")) {
        object_t *obj = vm_pop(vm);
        if (obj->type == &nlist_type) {
            vm_push(vm, object_create_nlist(nlist_copy(obj->data.ptr)));
        } else if (obj->type == &list_type) {
            vm_push(vm, object_create_nlist(nlist_from_list(obj->data.ptr)));
        } else {
            list_t *list = list_create();
            object_t *next_obj;
            while (next_obj = object_next(obj, vm)) list_push(list, next_obj);
            vm_push(vm, object_create_nlist(nlist_from_list(list)));
        }
    } else if (!strcmp(name, "zeros")) {
        int len = object_to_int(vm_pop(vm));
        nlist_t *nlist = nlist_create(len);
        for (int i = 0; i < len; i++) nlist->elems[i] = 0;
        vm_push(vm, object_create_nlist(nlist));
    } else if (!strcmp(name, "ones")) {
        int len = object_to_int(vm_pop(vm));
        nlist_t *nlist = nlist_create(len);
        for (int i = 0; i < len; i++) nlist->elems[i] = 1;
        vm_push(vm, object_create_nlist(nlist));
    } else return false;
    return true;
}

static object_t *nlist_next(iterator_t *it, vm_t *vm) {
    nlist_t *nlist = it->data.custom.data;
    return vm_get_or_create_int(vm, nlist->elems[it->i]);
}

bool nlist_getter(object_t *self, const char *name, vm_t *vm) {
    nlist_t *nlist = self->data.ptr;
    int op;
    if (!strcmp(name, "len")) {
        vm_push(vm, vm_get_or_create_int(vm, nlist->len));
    } else if (!strcmp(name, "__iter__")) {
        iterator_t *it = iterator_create(ITER_CUSTOM, nlist->len,
            (iterator_data_t){ .custom = { .next = nlist_next, .data = nlist } });
        vm_push(vm, object_create_iterator(it));
    } else if (!strcmp(name, "slice")) {
        object_t *end_obj = vm_pop(vm);
        int end = end_obj == &static_null? nlist->len: object_to_int(end_obj);
        int start = object_to_int(vm_pop(vm));
        iterator_t *it = iterator_create_slice(ITER_CUSTOM, nlist->len,
            (iterator_data_t){ .custom = { .next = nlist_next, .data = nlist } },
            start, end);
        vm_push(vm, object_create_iterator(it));
    } else if (!strcmp(name, "copy")) {
        vm_push(vm, object_create_nlist(nlist_copy(nlist)));
    } else if (!strcmp(name, "get")) {
        object_t *i_obj = vm_pop(vm);
        int i = object_to_int(i_obj);
        vm_push(vm, vm_get_or_create_int(vm, nlist_get(nlist, i)));
    } else if (!strcmp(name, "set")) {
        int i = object_to_int(vm_pop(vm));
        int value = object_to_int(vm_pop(vm));
        nlist_set(nlist, i, value);
    } else if (!strcmp(name, "to_list")) {
        list_t *list = nlist_to_list(nlist, vm);
        vm_push(vm, object_create_list(list));
    } else if (
        op = parse_operator(name),
        op >= FIRST_INT_OP && op <= LAST_BOOL_OP
    ) {
        instruction_t instruction = FIRST_OP_INSTR + op;
        bool is_unop = op_arities[op] == 1;
        if (is_unop) {
            int len = nlist->len;
            for (int i = 0; i < len; i++) nlist->elems[i] = int_op(op, nlist->elems[i], 0);
        } else {
            object_t *other = vm_top(vm);
            if (other->type == &int_type) {
                vm->stack_top--;
                int j = other->data.i;
                int len = nlist->len;
                for (int i = 0; i < len; i++) nlist->elems[i] = int_op(op, nlist->elems[i], j);
            } else if (other->type == &list_type) {
                vm->stack_top--;
                list_t *other_list = other->data.ptr;
                int len = MIN(nlist->len, other_list->len);
                for (int i = 0; i < len; i++) nlist->elems[i] = int_op(op, nlist->elems[i],
                    object_to_int(other_list->elems[i]));
            } else if (other->type == &nlist_type) {
                vm->stack_top--;
                nlist_t *other_nlist = other->data.ptr;
                int len = MIN(nlist->len, other_nlist->len);
                for (int i = 0; i < len; i++) nlist->elems[i] = int_op(op, nlist->elems[i],
                    other_nlist->elems[i]);
            } else {
                int len = nlist->len;
                object_t *obj_it = vm_iter(vm);
                object_t *next_obj;
                for (int i = 0; i < len && (next_obj = object_next(obj_it, vm)); i++) {
                    nlist->elems[i] = int_op(op, nlist->elems[i], object_to_int(next_obj));
                }
            }
        }
        vm_push(vm, self);
    } else return false;
    return true;
}

type_t nlist_type = {
    .name = "nlist",
    .print = nlist_print,
    .type_getter = nlist_type_getter,
    .getter = nlist_getter,
};

void nlist_init(vm_t *vm) {
    // NOTE: this function is called by @cimport
    dict_set(vm->globals, "nlist", object_create_type(&nlist_type));
}
