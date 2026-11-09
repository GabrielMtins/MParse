#ifndef MPARSER_H
#define MPARSER_H

#include "stb_ds.h"

#define MPARSE_IMPLEMENTATION

#define MP_FUNC1(stack, func) \
	do { \
		float a = arrpop(stack); \
		float res = func(a); \
		arrpush(stack, res); \
	} while(0)

#define MP_FUNC2(stack, func) \
	do { \
		float a = arrpop(stack); \
		float b = arrpop(stack); \
		float res = func(b, a); \
		arrpush(stack, res); \
	} while(0)

#define MP_FUNC3(stack, func) \
	do { \
		float a = arrpop(stack); \
		float b = arrpop(stack); \
		float c = arrpop(stack); \
		float res = func(c, b, a); \
		arrpush(stack, res); \
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
		const float *variable;
		int operation;
	} data;
} MP_Token;

typedef struct MP_Var {
	const char *name;
	const float *address;
} MP_Var;

typedef MP_Token * MP_TokenList;

typedef struct MP_Expr {
	MP_TokenList output;
} MP_Expr;

#ifdef MP_DEBUG
void MP_DebugPrintToken(const MP_Token *token);
void MP_DebugPrintTokenList(MP_TokenList list);
void MP_DebugPrintExpr(const MP_Expr *expr);
#endif

MP_Expr MP_Compile(const char *str, const MP_Var *vars, int num_vars, int *error);
void MP_Free(MP_Expr *expr);
float MP_Eval(const MP_Expr *expr);

#ifdef MPARSE_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

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
	DO(MP_MFUNC_COS, cosf,   MP_FUNC1, "cos",  5) \
	DO(MP_MFUNC_SIN, sinf,   MP_FUNC1, "sin",  5) \
	DO(MP_MFUNC_TAN, tanf,   MP_FUNC1, "tan",  5) \
	DO(MP_MFUNC_LOG, log10f, MP_FUNC1, "log",  5) \
	DO(MP_MFUNC_LN,  logf,   MP_FUNC1, "ln",   5) \
	DO(MP_MFUNC_EXP, expf,   MP_FUNC1, "exp",  5) \
	DO(MP_MFUNC_ABS, fabsf,  MP_FUNC1, "abs",  5) \
	DO(MP_MFUNC_POW, powf,   MP_FUNC2, "pow",  5)

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

typedef float * FloatVector;
typedef char * CharVector;

static CharVector MP_CleanWhitespace(const char *str);
static bool MP_FindToken(const char *str, MP_Token *token, int *jump_len);
static bool MP_FindNumber(const char *str, int len, MP_Token *token);
static bool MP_FindVar(const char *str, int len, const MP_Var *vars, int num_vars, MP_Token *token);
static bool MP_FindVarNum(const char *str, int i, int prev_token_offset, const MP_Var *vars, int num_vars, MP_Token *token);
static MP_TokenList MP_ParseTokens(const char *str, const MP_Var *vars, int num_vars, int *error);
static void MP_CheckMatchingBrackets(const MP_TokenList list, int *error);
static void MP_CheckUnaryOperators(MP_TokenList list);
static void MP_ValidateRPN(const MP_TokenList list, int *error);
static MP_TokenList MP_InfixToRPN(const MP_TokenList input_list);
static void MP_DoOperation(FloatVector result, int op);

static CharVector MP_CleanWhitespace(const char *str) {
	CharVector cleaned = NULL;
	int len;

	len = strlen(str);

	for(int i = 0; i < len; i++) {
		if(isspace(str[i])) {
			continue;
		}

		arrput(cleaned, str[i]);
	}

	arrput(cleaned, '\0');

	return cleaned;
}

static bool MP_FindToken(const char *str, MP_Token *token, int *jump_len) {
	int best_match_index = -1;
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
			best_match_index = i;
		}
	}

	if(best_match_index < 0) {
		return false;
	}

	token->type = MP_TOKENTYPE_OPERATION;
	token->data.operation = best_match_index;
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
		num = 10 * num + (int) (str[i] - '0');
	}

	for(int i = 0; i < num_div_by_ten; i++) {
		num /= 10;
	}

	token->type = MP_TOKENTYPE_NUMBER;
	token->data.number = num;

	return true;
}

static bool MP_FindVar(const char *str, int len, const MP_Var *vars, int num_vars, MP_Token *token) {
	for(int i = 0; i < num_vars; i++) {
		if((int) strlen(vars[i].name) != len) {
			continue;
		}

		if(strncmp(str, vars[i].name, len) != 0) {
			continue;
		}

		token->type = MP_TOKENTYPE_VARIABLE;
		token->data.variable = vars[i].address;

		return true;
	}

	return false;
}

static bool MP_FindVarNum(const char *str, int i, int prev_token_offset, const MP_Var *vars, int num_vars, MP_Token *token) {
	const char *start_str = str + prev_token_offset;
	int len = i - prev_token_offset;

	if(MP_FindNumber(start_str, len, token)) {
		return true;
	}

	if(MP_FindVar(start_str, len, vars, num_vars, token)) {
		return true;
	}

	return false;
}

static MP_TokenList MP_ParseTokens(const char *str, const MP_Var *vars, int num_vars, int *error) {
	MP_TokenList list = NULL;

	int i, prev_token_offset, len;

	i = 0;
	prev_token_offset = 0;
	len = strlen(str);

	while(i < len) {
		MP_Token token;
		int jump_len;

		if(!MP_FindToken(str + i, &token, &jump_len)) {
			i++;
			continue;
		}

		if(i > prev_token_offset) {
			MP_Token token_num;

			bool found = MP_FindVarNum(
					str,
					i,
					prev_token_offset,
					vars,
					num_vars,
					&token_num
					);

			if(!found) {
				*error = MP_ERROR_UNKNOWN_VARIABLE;
				return list;
			}

			arrput(list, token_num);
		}

		prev_token_offset = i + jump_len;

		arrput(list, token);

		i += jump_len;
	}

	if(i > prev_token_offset) {
		MP_Token token_num;

		bool found = MP_FindVarNum(
				str,
				i,
				prev_token_offset,
				vars,
				num_vars,
				&token_num
				);

		if(!found) {
			*error = MP_ERROR_UNKNOWN_VARIABLE;
			return list;
		}

		arrput(list, token_num);
	}

	return list;
}

static void MP_CheckMatchingBrackets(const MP_TokenList list, int *error) {
	int num_brackets = 0;

	for(int i = 0; i < arrlen(list); i++) {
		if(list[i].type == MP_TOKENTYPE_OPEN_BRACKET) {
			num_brackets++;
		} else if(list[i].type == MP_TOKENTYPE_CLOSE_BRACKET) {
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

static void MP_CheckUnaryOperators(MP_TokenList list) {
	MP_Token *prev = NULL, *curr = NULL;

	for(int i = 0; i < arrlen(list); i++) {
		if(i != 0) prev = &list[i - 1];
		curr = &list[i];

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

static void MP_ValidateRPN(const MP_TokenList list, int *error) {
	int depth = 0, arity = 0;

	for(int i = 0; i < arrlen(list); i++) {
		const MP_Token *token = &list[i];

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
	}

	if(depth != 1) {
		*error = MP_ERROR_INVALID_EXPRESSION;
	}
}

static MP_TokenList MP_InfixToRPN(const MP_TokenList input_list) {
	MP_TokenList operation_stack = NULL;
	MP_TokenList output = NULL;

	for(int i = 0; i < arrlen(input_list); i++) {
		const MP_Token *current = &input_list[i];

		switch(current->type) {
			case MP_TOKENTYPE_OPEN_BRACKET:
				arrput(operation_stack, *current);
				break;

			case MP_TOKENTYPE_VARIABLE:
			case MP_TOKENTYPE_NUMBER:
				arrput(output, *current);
				break;

			case MP_TOKENTYPE_OPERATION:
				while(1) {
					if(arrlen(operation_stack) == 0 || arrlast(operation_stack).type == MP_TOKENTYPE_OPEN_BRACKET) {
						arrput(operation_stack, *current);
						break;
					}

					MP_Token *top = &arrlast(operation_stack);

					if(mp_op_prec[current->data.operation] > mp_op_prec[top->data.operation]) {
						arrput(operation_stack, *current);
						break;
					} else {
						arrpush(output, *top);
						arrpop(operation_stack);
					}
				}
				break;

			case MP_TOKENTYPE_CLOSE_BRACKET:
				while(arrlen(operation_stack) > 0) {
					if(arrlast(operation_stack).type == MP_TOKENTYPE_OPEN_BRACKET) {
						arrpop(operation_stack);
						break;
					}

					arrpush(output, arrpop(operation_stack));
				}
				break;

			default:
				break;
		}
	}

	while(arrlen(operation_stack) > 0) {
		arrpush(output, arrpop(operation_stack));
	}
	
	arrfree(operation_stack);

	return output;
}

static void MP_DoOperation(FloatVector result, int op) {
	#define EXPAND_AS_CASE(id, func, macro_num_args, ...) \
		case id: macro_num_args(result, func); break;
	switch(op) {
		FOR_MP_OP(EXPAND_AS_CASE)
		FOR_MP_MFUNC(EXPAND_AS_CASE)
		#ifdef FOR_MP_CFUNC
		#define CUSTOM_EXPAND_AS_CASE(func, macro_num_args, ...) \
			case MP_CFUNC_ID(func): macro_num_args(result, func); break;
		FOR_MP_CFUNC(CUSTOM_EXPAND_AS_CASE)
		#undef CUSTOM_EXPAND_AS_CASE
		#endif
	}
	#undef EXPAND_AS_CASE
}

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
			printf("var[%p]", (void *) token->data.variable);
			break;
	}
}

void MP_DebugPrintTokenList(MP_TokenList list) {
	for(int i = 0; i < arrlen(list); i++) {
		MP_DebugPrintToken(&list[i]);
		printf(" ");
	}
	printf("\n");
}

void MP_DebugPrintExpr(const MP_Expr *expr) {
	MP_DebugPrintTokenList(expr->output);
}

MP_Expr MP_Compile(const char *str, const MP_Var *vars, int num_vars, int *error) {
	MP_Expr expr;

	MP_TokenList input_list = NULL;
	CharVector cleaned_str = NULL;
	int internal_error = 0;

	cleaned_str = MP_CleanWhitespace(str);
	expr.output = NULL;

	input_list = MP_ParseTokens(cleaned_str, vars, num_vars, &internal_error);
	if(internal_error) {
		goto mp_compile_end;
	}

	MP_CheckMatchingBrackets(input_list, &internal_error);
	if(internal_error) {
		goto mp_compile_end;
	}

	MP_CheckUnaryOperators(input_list);

	expr.output = MP_InfixToRPN(input_list);

	MP_ValidateRPN(expr.output, &internal_error);

	if(internal_error) {
		arrfree(expr.output);
		expr.output = NULL;
		goto mp_compile_end;
	}

mp_compile_end:
	if(error != NULL) *error = internal_error;

	arrfree(input_list);
	arrfree(cleaned_str);

	return expr;
}

void MP_Free(MP_Expr *expr) {
	arrfree(expr->output);
}

float MP_Eval(const MP_Expr *expr) {
	int len;
	FloatVector result = NULL;
	float final_res = 0.0f;

	len = arrlen(expr->output);

	if(len == 0) {
		return 0.0f;
	}

	for(int i = 0; i < len; i++) {
		const MP_Token *token = &expr->output[i];

		switch(token->type) {
			case MP_TOKENTYPE_NUMBER:
				arrput(result, token->data.number);
				break;

			case MP_TOKENTYPE_VARIABLE:
				arrput(result, *token->data.variable);
				break;

			case MP_TOKENTYPE_OPERATION:
				MP_DoOperation(result, token->data.operation);
				break;

			default:
				break;
		}
	}

	final_res = arrlast(result);

	arrfree(result);

	return final_res;
}

#endif /* MPARSE_IMPLEMENTATION */

#endif
