# MParse

MParse is a small, header-only C99 library for parsing and evaluating
mathematical expressions. It compiles an expression once into Reverse Polish
Notation (RPN), then it can evaluate it.

The library is designed for use cases where the same expression is evaluated
many times with different variable values, such as plotting graphics. Thus,
evaluating an expression is much cheaper than compiling it.

The library only depends on the **C standard library**.

MParse is distributed under the **zlib license**.

# Features

The library supports the following operations:

- Binary operators: `+`, `-`, `*`, `/`
- Unary operators: `+`, `-`
- Functions: `cos`, `sin`, `tan`, `log` (base 10), `ln`, `exp`, `abs`, `sqrt`,
  `cbrt`, `pow`

The library can also be extended with custom functions via x-macros.

It also supports variables, declared by name in `MP_Compile` and passed by
value in `MP_Eval`. The library only uses float.

It also compiles in C99, with `-Wall -Wextra -pedantic` without warnings on
**GCC** and **Clang**.

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
    float value = MP_Eval(&expr, NULL);
    MP_Free(&expr);
}
```

The expression read by `MP_Eval` must be a valid expression, compiled with no
errors. If it is not a valid expression, it will return 0.0f as a fallback.

MParse is written in C99 and its implementation is not compatible with standard
C++. The implementation **must be in a C source file if you use it on a C++
project**.

# Variables

`MP_Compile` accepts a `MP_StrArray`, an array of strings containing the names
of the variables used in the expression. `MP_StrArray` is a `typedef const char
*const *`.

`MP_Eval` accepts a `const float *`, an array of floats containing the values
of each variable matching the order of the names passed via `MP_StrArray`.

```c
const char *const var_names[] = {
	"x", "y"
};

float var_list[2];

MP_Expr expr = MP_Compile("x*y", var_names, 2, NULL);

var_list[0] = 0.5f;
var_list[1] = 3.0f;
float v1 = MP_Eval(&expr, var_list); // 1.5f

var_list[0] = 0.5f;
var_list[1] = 2.5f;
float v2 = MP_Eval(&expr, var_list); // 1.25f
```

`var_list` and `var_names` must have the same size, or else the behavior is
undefined (UB).

Passing the variable values on the `MP_Eval` function means that this library
is **thread-safe**. Once the expression is compiled, it can be evaluated in
multiple threads without locking if each thread has its own `var_list`.

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
    float value = MP_Eval(&expr, NULL);
    MP_Free(&expr);
}
```
