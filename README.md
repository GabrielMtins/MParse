# MParse

MParse is a small, header-only C99 library for parsing and evaluating
mathematical expressions. It compiles an expression once into Reverse Polish
Notation (RPN), then it can evalute it.

The library is designed for use cases where the same expression is evaluated
many times with different variable values, such as plotting graphics. Thus,
evaluating an expression is much cheaper than compiling it.

The library only depends on the **C standard library**.

MParse is distributed under the **zlib license**.

# Design

The library supports the following operations:

- Binary operators: `+`, `-`, `*`, `/`
- Unary operators: `+`, `-`
- Functions: `cos`, `sin`, `tan`, `log` (base 10), `ln`, `exp`, `abs`, `sqrt`,
  `cbrt`, `pow`

The library can also be extended with custom functions via x-macros.

It also supports custom variables bound by name to a `float *`. The library
**only uses floats**.

It also compiles in C99, with `-Wall -Wextra -pedantic` without warnings.

# Usage

In exactly one source file, define `MPARSE_IMPLEMENTATION` before including
the header:

```c
#define MPARSE_IMPLEMENTATION
#include "MParse.h"
```

Example usage of compiling and evaluating an expression:
```c
int error;

MP_Expr expr = MP_Compile("2 + 3 * 4", NULL, 0, &error);

if(error == 0) {
    float value = MP_Eval(&expr);
    MP_Free(&expr);
}
```

# Variables

`MP_Compile` accepts an array of `MP_Var`, each binding a name to a `float *`.
The value is read at evaluation time, so it can change between calls to
`MP_Eval`:

```c
float x = 2.0f;
MP_Var vars[] = {
    { "x", &x },
};

int error = 0;
MP_Expr expr = MP_Compile("x * x + 1", vars, 1, &error);
/* ... */

float a = MP_Eval(&expr);  /* 5.0f */

x = 3.0f;
float b = MP_Eval(&expr);  /* 10.0f */

MP_Free(&expr);
```

The address of the variable must **not** be NULL, or else the program will seg
fault. It does not check if the address is NULL at evaluation time.

# Extending with custom functions

Custom functions can be added by defining `FOR_MP_CFUNC` before including the
implementation. The macro is a table (x-macro) that is expanded for each custom
function. It takes three parameters:

1. A C function that returns `float` and takes 1 to 3 parameters.
2. The macro corresponding to the number of arguments the function takes:
   `MP_FUNC1`, `MP_FUNC2`, `MP_FUNC3`.
3. The name used inside the expression.

Here's an example:
```c
float lerp(float from, float to, float i);
float compute(float x);

/* On implementation file */
#define FOR_MP_CFUNC(DO) \
    DO(lerp,    MP_FUNC3, "lerp") \
    DO(compute, MP_FUNC1, "compute")

#define MPARSE_IMPLEMENTATION
#include "MParse.h"

/* ... */

/* Compiling an expression on another file */
int error;

MP_Expr expr = MP_Compile("lerp(compute(0.1), compute(3.5), 0.5)", NULL, 0, &error);

if(error == 0) {
    float value = MP_Eval(&expr);
    MP_Free(&expr);
}
```
