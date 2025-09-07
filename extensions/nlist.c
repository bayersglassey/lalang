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
    return list;

    nlist_t *nlist = nlist_create(list->len);
    for (int i = 0; i < list->len; i++) {
        nlist->elems[i] = object_to_int(list->elems[i]);
    }
    return nlist;
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
            fprintf(stderr, "TODO: implement iterators... (tried to call @nlist on an '%s' object)\n", obj->type->name);
            exit(1);
        }
    } else if (!strcmp(name, "zeros")) {
        int len = object_to_int(vm_pop(vm));
        nlist_t *nlist = nlist_create(len);
        vm_push(vm, object_create_nlist(nlist));
    } else if (!strcmp(name, "ones")) {
        int len = object_to_int(vm_pop(vm));
        nlist_t *nlist = nlist_create(len);
        for (int i = 0; i < len; i++) nlist->elems[i] = 1;
        vm_push(vm, object_create_nlist(nlist));
    } else return false;
    return true;
}

bool nlist_getter(object_t *self, const char *name, vm_t *vm) {
    nlist_t *nlist = self->data.ptr;
    if (!strcmp(name, "len")) {
        vm_push(vm, vm_get_or_create_int(vm, nlist->len));
    } else if (!strcmp(name, "copy")) {
        vm_push(vm, object_create_nlist(nlist_copy(nlist)));
    } else if (!strcmp(name, "get")) {
        object_t *i_obj = vm_pop(vm);
        int i = object_to_int(i_obj);
        vm_push(vm, vm_get_or_create_int(vm, nlist_get(nlist, i)));
    } else if (!strcmp(name, "set")) {
        int i = object_to_int(vm_pop(vm));
        object_t *value = object_to_int(vm_pop(vm));
        nlist_set(nlist, i, value);
    } else if (!strcmp(name, "to_list")) {
        list_t *list = nlist_to_list(nlist);
        vm_push(vm, object_create_list(list));
    } else {
        /* TODO: implement operators... */
        return false;
    }
    return true;
}

type_t nlist_type = {
    .name = "nlist",
    .print = nlist_print,
    .type_getter = nlist_type_getter,
    .getter = nlist_getter,
};

void nlist_cimport(vm_t *vm) {
    // NOTE: this function is called by @cimport
    dict_set(vm->globals, "nlist", object_create_type(&nlist_type));
}
