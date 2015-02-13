# Contributing to mongo-c-driver

Thanks for considering contributing to the mongo-c-driver!

This document intends to be a short guide to helping you contribute to the codebase.
It expects a familiarity with the C programming language and writing portable software.
Whenever in doubt, feel free to ask others that have contributed or look at the existing body of code.


## Guidelines

The mongo-c-driver has a few guidelines that help direct the process.


### Portability

mongo-c-driver is portable software. It needs to run on a multitude of
operating systems and architectures.

 * Linux (RHEL 5 and newer)
 * FreeBSD (10 and newer)
 * Windows (Vista a newer)
 * Solaris x86_64/SPARC (11 and newer)
 * SmartOS (Solaris based)
 * Possibly more if users show an interest.
 * ARM/SPARC/x86/x86_64


### Licensing

Some of the mongo-c-driver users embed the library statically in their
products.  Therefore, the driver needs to be liberally licensed (as opposed to
the authors usual preference of LGPL-2+). Therefore, all contributions must
also be under this license. As a policy, we have chosen Apache 2.0 as the
license for the project.


### Coding Style

We try not to be pedantic with taking contributions that are not properly
formatted, but we will likely perform a followup commit that cleans things up.
The basics are, in vim:

 : set ts=3 sw=3 et

3 space tabs, insert spaces instead of tabs.

Place a space between the function name and the parameter as such:

```c
static void
my_func (Param *p)

my_func (p);
```

Not all of the code does this today, but it should be cleaned up at some point.

Just look at the code around for more pedantic styling choices.


### Enum, Struct, Variable Naming

The naming conventions for mongo-c-driver should feel very object oriented.
In fact, mongo-c-driver is OOP. Those that have used the GLib library will
feel right at home, as the author has spent many years contributing to that
project as well.

Structs are suffixed in `_t`, and underscores.

```c
typedef struct _my_struct_t my_struct_t;

struct _my_struct_t
{
   int foo;
};
```

Function names should be prefixed by the type name, without the `_t`.

```c
int my_struct_get_foo (my_struct_t *my);
```

Enums are also named with the `_t` suffix.


```c
typedef enum
{
   MY_FLAGS_A = 1,
   MY_FLAGS_B = 1 << 1,
   MY_FLAGS_C = 1 << 2,
} my_flags_t;
```

### Adding a new symbol

This should be done rarely but there are several things that you need to do
when adding a new symbol.

 - Add the symbol to `src/libmongoc.symbols`
 - Add the symbol to `build/autotools/versions.ldscript`
 - Add the symbol to `build/cmake/libmongoc.def`
 - Add the symbol to `build/cmake/libmongoc-ssl.def`
 - Add documentation for the new symbol in `doc/mongoc_your_new_symbol_name.page`

### Documentation

We strive to document all symbols. See doc/ for documentation examples. If you
add a new function, add a new .txt file describing the function so that we can
generate man pages and HTML for it.


### Testing

You should always run `make test` before submitting a patch. Just make sure you
have a locally running `mongod` instance available on `127.0.0.1:27017`. All
tests should pass. Alternatively, you can specify `MONGOC_TEST_HOST`
environment variable to specify a non-localhost hostname or ip address.

All tests should pass before submitting a patch.

