# LALANG

It's a small, explicitly stack-based, interpreted language, implemented in C.
Basically, it's bytecode with a REPL!

The name comes from "lala", my standard easy-to-type nonsense word for when I need
to make a throwaway file or directory.


## Motivational examples

Basic math and functions! I hope you like [reverse Polish notation](https://en.wikipedia.org/wiki/Reverse_Polish_notation)!

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

Strings! Slices! Iterators!

```
>>> "hello!" @sorted @join @print
"!ehllo"
>>> 3 8 "Hello world!" .slice =s
>>> s @print
<str iterator at 0x5682b77d2e10>
>>> s @list
["l", "o", " ", "w", "o"]
```

More iterators! Ranges! Maps! Enumerations! Zippers! Destructuring!

```
>>> ( ( 0 10 @range ) double @map ) @list @print
[0, 2, 4, 6, 8, 10, 12, 14, 16, 18]
>>> "Yes!" @enumerate @list @print
[[0, "Y"], [1, "e"], [2, "s"], [3, "!"]]
>>> "Yes!" "No!" @zip { .unpair + } @map @list @join
"YNeos!"
```

Classes! They work pretty similar to Python, everybody loves Python!

```
>>> # Create a new class
>>> "Tree" @class =Tree
>>>
>>> # Implement the "__init__" method
>>> [
...     =self
...     self =.tag
...     list .new self =.children
...     self
... ] $__init__ Tree .set_getter
>>>
>>> # Implement the "__print__" method
>>> [
...     =self
...     self .tag .write "(" .write
...     true =first
...     {
...         first { false =first } { ", " .write } @ifelse
...         @print_inline
...     } self .children @for
...     ")" .write
... ] $__print__ Tree .set_getter
>>>
>>> # Implement the "," operator
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
A(1(i(), ii()), 2())
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


## A look at the Bytecode

Let's look at how a small program is tokenized, parsed, and evaluated:

```
$ echo '{ 2 * } =@f 5 @f' | QUIET=1 PRINT_TOKENS=1 ./lalang
GOT TOKEN: [{]
GOT TOKEN: [2]
GOT TOKEN: [*]
GOT TOKEN: [}]
GOT TOKEN: [=@f]
GOT TOKEN: [5]
GOT TOKEN: [@f]

$ echo '{ 2 * } =@f 5 @f' | QUIET=1 PRINT_CODE=1 ./lalang
=== PARSED CODE:
LOAD_INT 2
MUL
=== END CODE
=== PARSED RUNNABLE CODE:
LOAD_FUNC 68
RENAME_FUNC f
STORE_GLOBAL f
LOAD_INT 5
CALL_GLOBAL f
=== END CODE

$ echo '{ 2 * } =@f 5 @f' | QUIET=1 PRINT_EVAL=1 ./lalang
LOAD_FUNC 68
RENAME_FUNC f
STORE_GLOBAL f
LOAD_INT 5
CALL_GLOBAL f
LOAD_INT 2
MUL
```

The instruction set is defined as a C enum, and in the bytecode, every "byte" (errr,
every int... I guess it's technically intcode) is either an instruction, or an integer
argument to the previous instruction:

```
$ grep -A3 "enum instruction {" lalang.h
enum instruction {
    INSTR_LOAD_INT,
    INSTR_LOAD_STR,
    INSTR_LOAD_FUNC,

$ grep -A3 "union bytecode {" lalang.h
union bytecode {
    instruction_t instruction;
    int i;
};
```

When an instruction takes an integer argument, it's generally used as an index into
one of the "caches" living on the VM:

```
$ grep cache lalang.h
    object_t *int_cache[VM_INT_CACHE_SIZE];
    dict_t *str_cache;
    object_t *char_cache[256];
    list_t *code_cache;
int vm_get_cached_str_i(vm_t *vm, const char *s);
object_t *vm_get_cached_str(vm_t *vm, const char *s);
    int *locals; // indexes into vm->str_cache indicating local variable names
```

...so for instance, when parsing a string literal, it's added to `vm->str_cache`,
and a LOAD_STR instruction is appended to the code, followed by the string's index
in the cache.
The same principle applies when parsing a codeblock or function literal, i.e. `{ ... }`
or `[ ... ]`: an entry is added to `vm->code_cache`, and a LOAD_FUNC instruction and
index are appended to the parent code.


## Implementation of Classes

We've seen how types can be implemented in C.
But how about classes, i.e. user-defined types?.. e.g.

```
>>> "A" @class =A
>>> A @print
<type 'A'>
>>> @A =a
>>> a @print
<'A' object at 0x5d2b0456d620>
>>> 3 a =.x
>>> a .x
3
```

We can define methods, too (but they're called "getters" and "setters"):

```
>>> [
...     =.message
...     "Updated message." .writeline
... ] $message A .add_setter
>>> [ .message .writeline ] $greet A .add_getter
>>> "Hello!" a =.message
Updated message.
>>> a .greet
Hello!
```

It may be useful to see the bytecode involved (note how `=.x` and `.x` correspond to `SETTER x`
and `GETTER x`):

```
$ echo '"A" @class @ =a 3 a =.x a .x @print' | QUIET=1 PRINT_EVAL=1 ./lalang
LOAD_STR "A"
CALL_GLOBAL class
CALL
STORE_GLOBAL a
LOAD_INT 3
LOAD_GLOBAL a
SETTER x
LOAD_GLOBAL a
GETTER x
CALL_GLOBAL print
3
```

Well, we know that all types are just `type_t` objects at the C level.
So, clearly the built-in `@class` function is dynamically allocating `type_t` objects, right?..
And attributes, getters, and setters are presumably stored in `dict_t` objects...
But we need a C struct to hold those. Enter `cls_t`:

```
$ grep -A13 "struct cls {" lalang.h
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
```

So now, when `@class` is called, it just needs to create a `cls_t`, and a `type_t`,
and stick them in an `object_t` of type `type`, and push that onto the VM's stack!..

```
$ grep -A3 "void builtin_class" vm.c
void builtin_class(vm_t *vm) {
    const char *name = object_to_str(vm_pop(vm));
    vm_push(vm, object_create_cls(name, vm));
}

$ grep -A14 "object_create_cls" objects.c
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
```

What function pointers does the dynamically allocated `type_t` use?..
That is, how does it implement the C interface used by the VM when evaluating
bytecode instructions?..
In fact, it uses C functions with a `cls_` prefix, like `cls_print` and `cls_getter`:

```
$ grep -A15 "type_t \*type_create_cls" objects.c
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
```

And what does e.g. `cls_getter` do?.. that is, what is the logic for `obj .x`
where `obj` is a class instance?..
Or rather, what is the behaviour of the bytecode `GETTER x`?..

```
$ grep -A19 "bool cls_getter" objects.c
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
```

...etc.
Long story short, `obj .x` will try the following:
* look for "x" in `self->data.ptr` (the instance's attributes)
* look for "x" in `cls->getters`
* look for "x" in `cls->class_attrs`


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

Next up: we fight Python & numpy for dominance in the field of data science programming.
