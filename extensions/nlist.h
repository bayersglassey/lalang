#ifndef _NLIST_H_
#define _NLIST_H_

#include "../lalang.h"


typedef struct nlist nlist_t;

struct nlist {
    int len;
    int *elems;
};

extern type_t nlist_type;

nlist_t *nlist_create(int len) {
nlist_t *nlist_from_list(list_t *list) {
list_t *nlist_to_list(nlist_t *nlist) {
nlist_t *nlist_copy(nlist_t *nlist) {
object_t *object_create_nlist(nlist_t *nlist) {
int nlist_get(nlist_t *nlist, int i) {
void nlist_set(nlist_t *nlist, int i, int value) {

#endif
