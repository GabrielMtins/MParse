/* LICENSE
Copyright (c) 2026 Gabriel Martins

This software is provided 'as-is', without any express or implied warranty. In
no event will the authors be held liable for any damages arising from the use of
this software.

Permission is granted to anyone to use this software for any purpose, including
commercial applications, and to alter it and redistribute it freely, subject to
the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim
that you wrote the original software. If you use this software in a product, an
acknowledgment in the product documentation would be appreciated but is not
required.
2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

/* QUICK DOCUMENTATION

USAGE

MParser is a header-only library for parsing and evaluating mathematical
expressions.

Define MPARSE_IMPLEMENTATION in a **single** C source file before
including the library:

	#define MPARSE_IMPLEMENTATION
	#include "MParse.h"

PUBLIC API
	
	MP_Compile: returns a MP_Expr. Args:
		const char *: valid C string with an expression.
		MP_StrArray: an array of variable names to be used in the expression.
		int num_vars: the size of the array.
		int *error: error for error handling. error == 0 means the expression
		compiled successfully.
		
	MP_Eval: returns the result of the expression. Args:
		const MP_Expr *expr: a valid expression with no compilations errors.
		const float *: an array of the variables matching the order of its
		names passed to the MP_Compile function.

	MP_Free: frees the memory used by MP_Expr. Args:
		MP_Expr *: A pointer to a compiled expression.

BINDING VARIABLES

You can bind variables in your C programs to names on the expression. Example:

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


EXTENDING WITH CUSTOM FUNCTIONS

You can extend the library by defining the macro FOR_MP_CFUNC before its
implementation. Example:

	float lerp(float from, float to, float i);
	float compute(float x);
	
	#define FOR_MP_CFUNC(DO) \
		DO(lerp,    MP_FUNC3, "lerp") \
		DO(compute, MP_FUNC1, "compute")

The first argument is the C function. The second argument is the number
of arguments the function takes (MP_FUNC1, MP_FUNC2, MP_FUNC3). The
third argument is the name of the function in the expression.
*/

#ifndef MPARSE_H
#define MPARSE_H

#ifdef __cplusplus
extern "C" {
#endif

#define MP_FUNC1(sp, func) \
	do { \
		float a = *(--(sp)); \
		float res = func(a); \
		*((sp)++) = res; \
	} while(0)

#define MP_FUNC2(sp, func) \
	do { \
		float a = *(--(sp)); \
		float b = *(--(sp)); \
		float res = func(b, a); \
		*((sp)++) = res; \
	} while(0)

#define MP_FUNC3(sp, func) \
	do { \
		float a = *(--(sp)); \
		float b = *(--(sp)); \
		float c = *(--(sp)); \
		float res = func(c, b, a); \
		*((sp)++) = res; \
	} while(0)

#define MP_FUNC1_ARITY 1
#define MP_FUNC2_ARITY 2
#define MP_FUNC3_ARITY 3

#define MP_ERROR_UNKNOWN_VARIABLE 1
#define MP_ERROR_UNMATCHED_BRACKETS 2
#define MP_ERROR_NOT_ENOUGH_OPERANDS 3
#define MP_ERROR_INVALID_EXPRESSION 4

typedef struct MP_Token {
	int type;

	union {
		float number;
		int variable_index;
		int operation;
	} data;
} MP_Token;

typedef struct MP_Var {
	const char *name;
	const float *address;
} MP_Var;

typedef struct MP_TokenArray {
	MP_Token *data;
	int len;
} MP_TokenArray;

typedef struct MP_Expr {
	MP_TokenArray output;
	int max_depth;
} MP_Expr;

#ifdef MPARSE_DEBUG
void MP_DebugPrintToken(const MP_Token *token);
void MP_DebugPrintTokenArray(const MP_TokenArray *list);
void MP_DebugPrintExpr(const MP_Expr *expr);
#endif

typedef const char *const *MP_StrArray;

MP_Expr MP_Compile(const char *str, MP_StrArray vars, int num_vars, int *error);
void MP_Free(MP_Expr *expr);
float MP_Eval(const MP_Expr *expr, const float *vars);

#ifdef __cplusplus
}
#endif

#endif /* MPARSE_H */

#ifdef MPARSE_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define MP_ACREATE(size) (MP_TokenArray) {.len = 0, .data = (MP_Token *) malloc((size) * sizeof(MP_Token))}
#define MP_APUSH(array, p) ((array).data[(array).len++] = p)
#define MP_APOP(array) ((array).data[--(array).len])
#define MP_ALAST(array) ((array).data[(array).len - 1])
#define MP_AFREE(array) do { free((array).data); (array).len = 0; (array).data = NULL; } while(0)

#define MP_MACRO_ADD(x, y) (x + y)
#define MP_MACRO_SUB(x, y) (x - y)
#define MP_MACRO_MUL(x, y) (x * y)
#define MP_MACRO_DIV(x, y) (x / y)
#define MP_MACRO_MINUS(x) (-x)
#define MP_MACRO_PLUS(x) (+x)

#define FOR_MP_OP(DO) \
	DO(MP_OP_ADD,   MP_MACRO_ADD,   MP_FUNC2, "+", 2) \
	DO(MP_OP_SUB,   MP_MACRO_SUB,   MP_FUNC2, "-", 2) \
	DO(MP_OP_MUL,   MP_MACRO_MUL,   MP_FUNC2, "*", 3) \
	DO(MP_OP_DIV,   MP_MACRO_DIV,   MP_FUNC2, "/", 3) \
	DO(MP_OP_MINUS, MP_MACRO_MINUS, MP_FUNC1, "-", 4) \
	DO(MP_OP_PLUS,  MP_MACRO_PLUS,  MP_FUNC1, "+", 4) 

#define FOR_MP_MFUNC(DO) \
	DO(MP_MFUNC_COS,  cosf,   MP_FUNC1, "cos",  5) \
	DO(MP_MFUNC_SIN,  sinf,   MP_FUNC1, "sin",  5) \
	DO(MP_MFUNC_TAN,  tanf,   MP_FUNC1, "tan",  5) \
	DO(MP_MFUNC_LOG,  log10f, MP_FUNC1, "log",  5) \
	DO(MP_MFUNC_LN,   logf,   MP_FUNC1, "ln",   5) \
	DO(MP_MFUNC_EXP,  expf,   MP_FUNC1, "exp",  5) \
	DO(MP_MFUNC_ABS,  fabsf,  MP_FUNC1, "abs",  5) \
	DO(MP_MFUNC_SQRT, sqrtf,  MP_FUNC1, "sqrt", 5) \
	DO(MP_MFUNC_CBRT, cbrtf,  MP_FUNC1, "cbrt", 5) \
	DO(MP_MFUNC_POW,  powf,   MP_FUNC2, "pow",  5)

#define MP_CFUNC_ID(func) MP_CFUNC_ID_##func

#define EXPAND_AS_ENUM(id, ...) id,
enum MP_Operation {
	FOR_MP_OP(EXPAND_AS_ENUM)
#ifdef FOR_MP_CFUNC
	#define CUSTOM_EXPAND_AS_ENUM(func, type, str) MP_CFUNC_ID(func),
	FOR_MP_CFUNC(CUSTOM_EXPAND_AS_ENUM)
	#undef CUSTOM_EXPAND_AS_ENUM
#endif
	FOR_MP_MFUNC(EXPAND_AS_ENUM)
	MP_NUM_OPERATIONS,
};
#undef EXPAND_AS_ENUM

enum MP_TokenType {
	MP_TOKENTYPE_OPERATION,
	MP_TOKENTYPE_VARIABLE,
	MP_TOKENTYPE_NUMBER,
	MP_TOKENTYPE_OPEN_BRACKET,
	MP_TOKENTYPE_CLOSE_BRACKET,
	MP_TOKENTYPE_COMMA,
	MP_TOKENTYPE_NONE,
};

#define EXPAND_AS_STRING(id, func, type, str, ...) str,
static const char *mp_op_str[] = {
	FOR_MP_OP(EXPAND_AS_STRING)
#ifdef FOR_MP_CFUNC
	#define CUSTOM_EXPAND_AS_STRING(func, type, str) str,
	FOR_MP_CFUNC(CUSTOM_EXPAND_AS_STRING)
	#undef CUSTOM_EXPAND_AS_STRING
#endif
	FOR_MP_MFUNC(EXPAND_AS_STRING)
};
#undef EXPAND_AS_STRING

#define EXPAND_AS_PREC(id, func, type, str, p) p,
static const int mp_op_prec[] = {
	FOR_MP_OP(EXPAND_AS_PREC)
#ifdef FOR_MP_CFUNC
	#define CUSTOM_EXPAND_AS_PREC(...) 5,
	FOR_MP_CFUNC(CUSTOM_EXPAND_AS_PREC)
	#undef CUSTOM_EXPAND_AS_PREC
#endif
	FOR_MP_MFUNC(EXPAND_AS_PREC)
};
#undef EXPAND_AS_PREC

#define EXPAND_AS_ARITY(id, func, type, str, p) type##_ARITY,
static const int mp_op_arity[] = {
	FOR_MP_OP(EXPAND_AS_ARITY)
#ifdef FOR_MP_CFUNC
	#define CUSTOM_EXPAND_AS_ARITY(func, type, str) type##_ARITY,
	FOR_MP_CFUNC(CUSTOM_EXPAND_AS_ARITY)
	#undef CUSTOM_EXPAND_AS_ARITY
#endif
	FOR_MP_MFUNC(EXPAND_AS_ARITY)
};
#undef EXPAND_AS_ARITY

static char * MP_CleanWhitespace(const char *str);
static bool MP_FindToken(const char *str, MP_StrArray vars, int num_vars, MP_Token *token, int *jump_len);
static bool MP_FindNumber(const char *str, int len, MP_Token *token);
static MP_TokenArray MP_ParseTokens(const char *str, MP_StrArray vars, int num_vars, int *error);
static void MP_CheckMatchingBrackets(const MP_TokenArray *list, int *error);
static void MP_CheckUnaryOperators(MP_TokenArray *list);
static void MP_ValidateRPN(const MP_TokenArray *list, int *max_depth, int *error);
static MP_TokenArray MP_InfixToRPN(const MP_TokenArray *input_list);
static void MP_DoOperation(float **sp, int op);

static char * MP_CleanWhitespace(const char *str) {
	char * cleaned = NULL;
	int len, count;

	len = strlen(str);
	count = 0;

	cleaned = (char *) malloc(len + 1);

	for(int i = 0; i < len; i++) {
		if(isspace(str[i])) {
			continue;
		}

		cleaned[count++] = str[i];
	}

	cleaned[count] = '\0';

	return cleaned;
}

static bool MP_FindToken(const char *str, MP_StrArray vars, int num_vars, MP_Token *token, int *jump_len) {
	int best_match_len = 0;

	token->type = MP_TOKENTYPE_NONE;

	if(str[0] == '(') {
		token->type = MP_TOKENTYPE_OPEN_BRACKET;
		*jump_len = 1;
		return true;
	}

	if(str[0] == ')') {
		token->type = MP_TOKENTYPE_CLOSE_BRACKET;
		*jump_len = 1;
		return true;
	}

	if(str[0] == ',') {
		token->type = MP_TOKENTYPE_COMMA;
		*jump_len = 1;
		return true;
	}

	for(int i = 0; i < MP_NUM_OPERATIONS; i++) {
		int len = strlen(mp_op_str[i]);

		if(strncmp(str, mp_op_str[i], len) != 0) continue;

		if(len > best_match_len) {
			best_match_len = len;

			token->type = MP_TOKENTYPE_OPERATION;
			token->data.operation = i;
		}
	}
	
	for(int i = 0; i < num_vars; i++) {
		int len = strlen(vars[i]);

		if(strncmp(str, vars[i], len) != 0) continue;

		if(len > best_match_len) {
			best_match_len = len;

			token->type = MP_TOKENTYPE_VARIABLE;
			token->data.variable_index = i;
		}
	}

	if(best_match_len == 0) {
		return false;
	}

	*jump_len = best_match_len;

	return true;
}

static bool MP_FindNumber(const char *str, int len, MP_Token *token) {
	bool found_point = false;
	int num_div_by_ten = 0;
	float num = 0;

	/* check if it's a number */
	for(int i = 0; i < len; i++) {
		bool is_digit, is_point;

		is_digit = isdigit(str[i]);
		is_point = (str[i] == '.');

		if(!is_digit && !is_point) return false; 

		if(is_point && i == len - 1) return false;
		if(is_point && i == 0) return false;

		if(is_point && found_point) return false;

		if(is_point) {
			found_point = true;
			num_div_by_ten = len - i - 1;
		}
	}

	for(int i = 0; i < len; i++) {
		if(str[i] == '.') continue;
		num = 10 * num + (str[i] - '0');
	}

	for(int i = 0; i < num_div_by_ten; i++) {
		num /= 10;
	}

	token->type = MP_TOKENTYPE_NUMBER;
	token->data.number = num;

	return true;
}

static MP_TokenArray MP_ParseTokens(const char *str, MP_StrArray vars, int num_vars, int *error) {
	MP_TokenArray list;
	int i, prev_token_offset, len;

	i = 0;
	prev_token_offset = 0;
	len = strlen(str);

	list = MP_ACREATE(len);

	while(i < len) {
		MP_Token token;
		int jump_len;

		if(!MP_FindToken(str + i, vars, num_vars, &token, &jump_len)) {
			i++;
			continue;
		}

		if(i > prev_token_offset) {
			MP_Token token_num;

			bool found = MP_FindNumber(
					str + prev_token_offset,
					i - prev_token_offset,
					&token_num
					);

			if(!found) {
				*error = MP_ERROR_UNKNOWN_VARIABLE;
				return list;
			}

			MP_APUSH(list, token_num);
		}

		prev_token_offset = i + jump_len;

		MP_APUSH(list, token);

		i += jump_len;
	}

	if(i > prev_token_offset) {
		MP_Token token_num;

		bool found = MP_FindNumber(
				str + prev_token_offset,
				i - prev_token_offset,
				&token_num
				);

		if(!found) {
			*error = MP_ERROR_UNKNOWN_VARIABLE;
			return list;
		}

		MP_APUSH(list, token_num);
	}

	return list;
}

static void MP_CheckMatchingBrackets(const MP_TokenArray *list, int *error) {
	int num_brackets = 0;

	for(int i = 0; i < list->len; i++) {
		if(list->data[i].type == MP_TOKENTYPE_OPEN_BRACKET) {
			num_brackets++;
		} else if(list->data[i].type == MP_TOKENTYPE_CLOSE_BRACKET) {
			num_brackets--;
		}

		if(num_brackets < 0) {
			break;
		}
	}

	if(num_brackets != 0) {
		*error = MP_ERROR_UNMATCHED_BRACKETS;
	}
}

static void MP_CheckUnaryOperators(MP_TokenArray *list) {
	MP_Token *prev = NULL, *curr = NULL;

	for(int i = 0; i < list->len; i++) {
		if(i != 0) prev = &list->data[i - 1];
		curr = &list->data[i];

		if(curr->type != MP_TOKENTYPE_OPERATION) {
			continue;
		}

		bool check = 
			prev == NULL ||
			prev->type == MP_TOKENTYPE_OPEN_BRACKET ||
			prev->type == MP_TOKENTYPE_COMMA ||
			prev->type == MP_TOKENTYPE_OPERATION
			;

		if(!(check)) {
			continue;
		}

		switch(curr->data.operation) {
			case MP_OP_ADD:
				curr->data.operation = MP_OP_PLUS;
				break;

			case MP_OP_SUB:
				curr->data.operation = MP_OP_MINUS;
				break;
		}
	}
}

static void MP_ValidateRPN(const MP_TokenArray *list, int *max_depth, int *error) {
	int depth = 0, arity = 0;
	*max_depth = 1;

	for(int i = 0; i < list->len; i++) {
		const MP_Token *token = &list->data[i];

		switch(token->type) {
			case MP_TOKENTYPE_VARIABLE:
			case MP_TOKENTYPE_NUMBER:
				depth++;
				break;

			case MP_TOKENTYPE_OPERATION:
				arity = mp_op_arity[token->data.operation];
				if(depth < arity) {
					*error = MP_ERROR_NOT_ENOUGH_OPERANDS;
					return;
				}
				depth -= arity - 1;
				break;

			default:
				break;
		}

		if(depth > *max_depth) *max_depth = depth;
	}

	if(depth != 1) {
		*error = MP_ERROR_INVALID_EXPRESSION;
	}
}

static MP_TokenArray MP_InfixToRPN(const MP_TokenArray *input_list) {
	MP_TokenArray operation_stack, output;

	operation_stack = MP_ACREATE(input_list->len);
	output = MP_ACREATE(input_list->len);

	for(int i = 0; i < input_list->len; i++) {
		const MP_Token *current = &input_list->data[i];

		switch(current->type) {
			case MP_TOKENTYPE_OPEN_BRACKET:
				MP_APUSH(operation_stack, *current);
				break;

			case MP_TOKENTYPE_VARIABLE:
			case MP_TOKENTYPE_NUMBER:
				MP_APUSH(output, *current);
				break;

			case MP_TOKENTYPE_OPERATION:
				while(1) {
					if(operation_stack.len == 0 || MP_ALAST(operation_stack).type == MP_TOKENTYPE_OPEN_BRACKET) {
						MP_APUSH(operation_stack, *current);
						break;
					}

					MP_Token *top = &MP_ALAST(operation_stack);

					if(mp_op_prec[current->data.operation] > mp_op_prec[top->data.operation]) {
						MP_APUSH(operation_stack, *current);
						break;
					} else {
						MP_APUSH(output, *top);
						(void) MP_APOP(operation_stack);
					}
				}
				break;

			case MP_TOKENTYPE_CLOSE_BRACKET:
				while(operation_stack.len > 0) {
					if(MP_ALAST(operation_stack).type == MP_TOKENTYPE_OPEN_BRACKET) {
						(void) MP_APOP(operation_stack);
						break;
					}

					MP_APUSH(output, MP_APOP(operation_stack));
				}
				break;

			case MP_TOKENTYPE_COMMA:
				while(operation_stack.len > 0) {
					if(MP_ALAST(operation_stack).type == MP_TOKENTYPE_OPEN_BRACKET) {
						break;
					}

					MP_APUSH(output, MP_APOP(operation_stack));
				}
				break;

			default:
				break;
		}
	}

	while(operation_stack.len > 0) {
		MP_APUSH(output, MP_APOP(operation_stack));
	}
	
	MP_AFREE(operation_stack);

	return output;
}

static void MP_DoOperation(float **sp, int op) {
	#define EXPAND_AS_CASE(id, func, macro_num_args, ...) \
		case id: macro_num_args(*sp, func); break;
	switch(op) {
		FOR_MP_OP(EXPAND_AS_CASE)
		#ifdef FOR_MP_CFUNC
		#define CUSTOM_EXPAND_AS_CASE(func, macro_num_args, ...) \
			case MP_CFUNC_ID(func): macro_num_args(*sp, func); break;
		FOR_MP_CFUNC(CUSTOM_EXPAND_AS_CASE)
		#undef CUSTOM_EXPAND_AS_CASE
		#endif
		FOR_MP_MFUNC(EXPAND_AS_CASE)
	}
	#undef EXPAND_AS_CASE
}

#ifdef MPARSE_DEBUG
void MP_DebugPrintToken(const MP_Token *token) {
	switch(token->type) {
		case MP_TOKENTYPE_OPEN_BRACKET:
			printf("(");
			break;

		case MP_TOKENTYPE_CLOSE_BRACKET:
			printf(")");
			break;

		case MP_TOKENTYPE_NUMBER:
			printf("%f", token->data.number);
			break;

		case MP_TOKENTYPE_OPERATION:
			printf("%s", mp_op_str[token->data.operation]);
			break;

		case MP_TOKENTYPE_VARIABLE:
			printf("var[%d]", token->data.variable_index);
			break;
	}
}

void MP_DebugPrintTokenArray(const MP_TokenArray *list) {
	for(int i = 0; i < list->len; i++) {
		MP_DebugPrintToken(&list->data[i]);
		printf(" ");
	}
	printf("\n");
}

void MP_DebugPrintExpr(const MP_Expr *expr) {
	MP_DebugPrintTokenArray(&expr->output);
}
#endif /* MPARSE_DEBUG */

MP_Expr MP_Compile(const char *str, MP_StrArray vars, int num_vars, int *error) {
	MP_Expr expr = {0};

	MP_TokenArray input_list;
	char * cleaned_str = NULL;
	int internal_error = 0;

	cleaned_str = MP_CleanWhitespace(str);

	input_list = MP_ParseTokens(cleaned_str, vars, num_vars, &internal_error);
	if(internal_error) {
		goto mp_compile_end;
	}

	MP_CheckMatchingBrackets(&input_list, &internal_error);
	if(internal_error) {
		goto mp_compile_end;
	}

	MP_CheckUnaryOperators(&input_list);

	expr.output = MP_InfixToRPN(&input_list);

	MP_ValidateRPN(&expr.output, &expr.max_depth, &internal_error);

	if(internal_error) {
		MP_AFREE(expr.output);
		goto mp_compile_end;
	}

mp_compile_end:
	if(error != NULL) *error = internal_error;
	free(cleaned_str);
	MP_AFREE(input_list);

	return expr;
}

void MP_Free(MP_Expr *expr) {
	MP_AFREE(expr->output);
}

float MP_Eval(const MP_Expr *expr, const float *vars) {
	if(expr->output.len == 0) {
		return 0.0f;
	}

	float stack[expr->max_depth], *sp;

	sp = stack;

	for(int i = 0; i < expr->output.len; i++) {
		const MP_Token *token = &expr->output.data[i];

		switch(token->type) {
			case MP_TOKENTYPE_NUMBER:
				*(sp++) = token->data.number;
				break;

			case MP_TOKENTYPE_VARIABLE:
				*(sp++) = vars[token->data.variable_index];
				break;

			case MP_TOKENTYPE_OPERATION:
				MP_DoOperation(&sp, token->data.operation);
				break;

			default:
				break;
		}
	}

	return stack[0];
}

#endif /* MPARSE_IMPLEMENTATION */
