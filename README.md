# LALANG

It's a small, explicitly stack-based, interpreted language, implemented in C.
Basically, it's bytecode with a REPL!

The name comes from "lala", my standard easy-to-type nonsense word for when I need
to make a throwaway file or directory.


## Motivational examples

Basic math and functions! I hope you like reverse Polish notation!

```
>>> 1 2 10
1
2
10
>>> *
1
20
>>> +
21
>>> =x
>>> x x +
42
>>> @drop
>>> { 2 * } =@double
>>> x @double
84
```

Lists and dictionaries!

```
>>> list .new 1 , 2 , 3 , =l
>>> l @print
[1, 2, 3]
>>> l .pop @print l @print
3
[1, 2]
>>> dict .new "x" 1 @pair , "y" 2 @pair , =d
>>> d @print
{x: 1, y: 2}
>>> "y" d .get @print
2
```

Strings and iterators!

```
>>> "hello!" @sorted @join @print
"!ehllo"
>>> 3 8 "Hello world!" .slice =s
>>> s @print
<str iterator at 0x5682b77d2e10>
>>> s @list
["l", "o", " ", "w", "o"]
```

More iterators! Maps! Enumerations! Zippers!

```
>>> ( ( 0 10 @range ) double @map ) @list @print
[0, 2, 4, 6, 8, 10, 12, 14, 16, 18]
>>> 3 8 "Hello world!" .slice =s
>>> s
<str iterator at 0x5682b77d2e10>
>>> @list @print
["l", "o", " ", "w", "o"]
>>> "Yes!" @enumerate @list @print
[["Y", 0], ["e", 1], ["s", 2], ["!", 3]]
>>> "Yes!" "No!" @zip { .unpair + } @map @list @join
"YNeos!"
```

Classes! They work pretty similar to Python, everybody loves Python!

```
>>> "Tree" @class =Tree
>>> [
...     =self
...     self =.tag
...     list .new self =.children
...     self
... ] $__init__ Tree .set_getter
>>> [
...     =self
...     self .tag .write "(" .write
...     true =first
...     {
...         first { false =first } { " " .write } @ifelse
...         @print_inline
...     } self .children @for
...     ")" .write
... ] $__print__ Tree .set_getter
>>> [ =self self .children .push self ] "," Tree .__getters__ .set
>>> 
>>> 
>>>     "A" @Tree
>>>         "1" @Tree
>>>             "i" @Tree ,
>>>             "ii" @Tree ,
>>>         ,
>>>         "2" @Tree ,
>>> =tree
>>> 
>>> tree @print
A(1(i() ii()) 2())
```


## Implementation Overview

The way types and objects work at the C level is based on Python's approach:
* A **type** is a C struct (`type_t`) with a name, some data (`void *`), and a bunch of
  function pointers.
* An **object** is C struct (`object_t`) with a type (`type_t *`) and some data (`void *`).

We implement some basic data structures in C:
* `list_t`: a list of `object_t *`
* `dict_t`: a mapping from `const char *` keys to `object_t *` values

Next we implement bytecode and a VM to run it on:
* `code_t`: stores bytecode.
* `compiler_t`: compiles text (`const char *`) to code (`code_t`).
* `vm_t`: the virtual machine, on which we execute code (`code_t`).
  It has a stack of values (a `list_t`), and some mappings for global and local
  variables (both `dict_t`).
  It also has some caches for common `object_t` values, e.g. a cache for small
  integers, a cache for strings, a cache for compiled code, etc.
* `func_t`: has a name, and either a `code_t *` (interpreted function) or a
  `void (*)(vm_t*)` (built-in function).

Then we implement a `type_t` for all the built-in types!
The most important are:
* `nulltype` (plus the singleton `null`)
* `bool` (plus singletons `true`, `false`)
* `int`
* `str`
* `list`
* `dict`
* `func`

Then we implement various built-in functions, and register them in the globals
when we create the VM.

Then we implement a "standard library" written in our language, which gets evaluated
when we create the VM.

Then we have a little `main` function which implements a simple REPL (Read-Eval-Print
Loop, i.e. interactive shell).

That's pretty much it!

Fun fact, we do no memory management! Once something's allocated, it's around forever!
Good thing we cache small integers and strings, eh...


## C extensions

Naturally, like numpy for Python, we need to support 3rd party extensions written in C.

And so, with the magic of `dlopen(3)`, we do!

Let's make an extension library called "nlist" (i.e. number list), which when imported,
registers a new `nlist` type, which allows blazing fast operations on lists of int.
(Blazing fast compared to using for-loops and object pointers in our interpreted language, anyway.)

```
$ grep -A3 "struct nlist" extensions/nlist.h
typedef struct nlist nlist_t;

struct nlist {
    int len;
    int *elems;
};

$ tail -n 11 extensions/nlist.c
type_t nlist_type = {
    .name = "nlist",
    .print = nlist_print,
    .type_getter = nlist_type_getter,
    .getter = nlist_getter,
};

void nlist_init(vm_t *vm) {
    // NOTE: this function is called by @include
    dict_set(vm->globals, "nlist", object_create_type(&nlist_type));
}
```

Okay, let's compile this to a shared object (.so) file:

```
$ gcc -shared -fPIC -o nlist.so extensions/nlist.c -ldl
$ objdump -t nlist.so | grep nlist
nlist.so:     file format elf64-x86-64
0000000000000000 l    df *ABS*	0000000000000000              nlist.c
0000000000001ba7 l     F .text	000000000000004a              nlist_next
0000000000001947 g     F .text	0000000000000260              nlist_type_getter
0000000000001519 g     F .text	00000000000000be              nlist_create
0000000000001788 g     F .text	000000000000006e              nlist_get
00000000000017f6 g     F .text	0000000000000075              nlist_set
00000000000015d7 g     F .text	000000000000007c              nlist_from_list
0000000000001bf1 g     F .text	0000000000000839              nlist_getter
000000000000186b g     F .text	00000000000000dc              nlist_print
0000000000001653 g     F .text	0000000000000092              nlist_to_list
000000000000242a g     F .text	0000000000000042              nlist_init
0000000000001753 g     F .text	0000000000000035              object_create_nlist
00000000000016e5 g     F .text	000000000000006e              nlist_copy
0000000000005140 g     O .data	0000000000000058              nlist_type
```

And now, let's load it up and play with it:

```
>>> "nlist" @include
>>> nlist @print
<type 'nlist'>
>>> 5 nlist .zeros @print
nlist([0, 0, 0, 0, 0])
>>> 0 10 @range @nlist @print
nlist([0, 1, 2, 3, 4, 5, 6, 7, 8, 9])
>>> 0 10 @range @nlist 5 * @print
nlist([0, 5, 10, 15, 20, 25, 30, 35, 40, 45])
>>> ( 0 10 @range @nlist ) ( 10 20 @range ) + @print
nlist([10, 12, 14, 16, 18, 20, 22, 24, 26, 28])
```
