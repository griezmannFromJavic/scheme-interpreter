#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef enum { VAR, LAMBDA, APPLY, INT_LITERAL, ADD, MULTIPLY, QUOTE, DEFINE } ExprType;

typedef struct Expr {
    ExprType type;
    union {
        char *var;  // Variable
        struct {
            char *param;
            struct Expr *body;
        } lambda;  // Lambda
        struct {
            struct Expr *func;
            struct Expr *arg;
        } apply;  // Application
        int int_value;  // Integer literal
        struct {
            struct Expr *left;
            struct Expr *right;
        } binop;  // Binary operation (add/multiply)
    } data;
} Expr;

typedef struct Environment {
    char *var;
    Expr *value;
    struct Environment *next;
} Environment;

Expr *make_var(char *var) {
    Expr *expr = malloc(sizeof(Expr));
    expr->type = VAR;
    expr->data.var = strdup(var);
    return expr;
}

Expr *make_lambda(char *param, Expr *body) {
    Expr *expr = malloc(sizeof(Expr));
    expr->type = LAMBDA;
    expr->data.lambda.param = strdup(param);
    expr->data.lambda.body = body;
    return expr;
}

Expr *make_apply(Expr *func, Expr *arg) {
    Expr *expr = malloc(sizeof(Expr));
    expr->type = APPLY;
    expr->data.apply.func = func;
    expr->data.apply.arg = arg;
    return expr;
}

Expr *make_int(int value) {
    Expr *expr = malloc(sizeof(Expr));
    expr->type = INT_LITERAL;
    expr->data.int_value = value;
    return expr;
}

Expr *make_binop(ExprType type, Expr *left, Expr *right) {
    Expr *expr = malloc(sizeof(Expr));
    expr->type = type;
    expr->data.binop.left = left;
    expr->data.binop.right = right;
    return expr;
}

Environment *env_create(char *var, Expr *value, Environment *next) {
    Environment *env = malloc(sizeof(Environment));
    env->var = strdup(var);
    env->value = value;
    env->next = next;
    return env;
}

Expr *env_lookup(Environment *env, char *var) {
    while (env != NULL) {
        if (strcmp(env->var, var) == 0) {
            return env->value;
        }
        env = env->next;
    }
    return NULL;  // Variable not found
}

Expr *eval(Expr *expr, Environment *env) {
    switch (expr->type) {
        case VAR: {
            Expr *value = env_lookup(env, expr->data.var);
            if (value == NULL) {
                fprintf(stderr, "Unbound variable: %s\n", expr->data.var);
                exit(EXIT_FAILURE);
            }
            return value;
        }
        case LAMBDA:
            return expr;
        case APPLY: {
            Expr *func = eval(expr->data.apply.func, env);
            Expr *arg = eval(expr->data.apply.arg, env);
            if (func->type != LAMBDA) {
                fprintf(stderr, "Attempt to apply non-lambda expression\n");
                exit(EXIT_FAILURE);
            }
            Environment *new_env = env_create(func->data.lambda.param, arg, env);
            return eval(func->data.lambda.body, new_env);
        }
        case INT_LITERAL:
            return expr;
        case ADD: {
            Expr *left = eval(expr->data.binop.left, env);
            Expr *right = eval(expr->data.binop.right, env);
            if (left->type != INT_LITERAL || right->type != INT_LITERAL) {
                fprintf(stderr, "Addition requires integer literals\n");
                exit(EXIT_FAILURE);
            }
            return make_int(left->data.int_value + right->data.int_value);
        }
        case MULTIPLY: {
            Expr *left = eval(expr->data.binop.left, env);
            Expr *right = eval(expr->data.binop.right, env);
            if (left->type != INT_LITERAL || right->type != INT_LITERAL) {
                fprintf(stderr, "Multiplication requires integer literals\n");
                exit(EXIT_FAILURE);
            }
            return make_int(left->data.int_value * right->data.int_value);
        }
        case QUOTE:
            return expr->data.apply.arg;
        case DEFINE: {
            char *var = expr->data.apply.func->data.var;
            Expr *value = eval(expr->data.apply.arg, env);
            env = env_create(var, value, env);
            return value;
        }
        default:
            fprintf(stderr, "Unknown expression type\n");
            exit(EXIT_FAILURE);
    }
}

char *read_token(char **input) {
    while (isspace(**input)) (*input)++;
    char *start = *input;
    if (isalpha(**input)) {
        while (isalnum(**input)) (*input)++;
    } else if (isdigit(**input)) {
        while (isdigit(**input)) (*input)++;
    } else if (**input == '(' || **input == ')' || **input == '+' || **input == '*') {
        (*input)++;
    } else {
        start = NULL;
    }
    int length = *input - start;
    char *token = malloc(length + 1);
    strncpy(token, start, length);
    token[length] = '\0';
    return token;
}

Expr *parse_expr(char **input);

Expr *parse_list(char **input) {
    char *token = read_token(input);
    if (strcmp(token, "lambda") == 0) {
        free(token);
        char *param = read_token(input);
        Expr *body = parse_expr(input);
        free(read_token(input));  // consume closing parenthesis
        return make_lambda(param, body);
    } else if (strcmp(token, "+") == 0) {
        free(token);
        Expr *left = parse_expr(input);
        Expr *right = parse_expr(input);
        free(read_token(input));  // consume closing parenthesis
        return make_binop(ADD, left, right);
    } else if (strcmp(token, "*") == 0) {
        free(token);
        Expr *left = parse_expr(input);
        Expr *right = parse_expr(input);
        free(read_token(input));  // consume closing parenthesis
        return make_binop(MULTIPLY, left, right);
    } else if (strcmp(token, "quote") == 0) {
        free(token);
        Expr *quoted_expr = parse_expr(input);
        free(read_token(input));  // consume closing parenthesis
        Expr *expr = malloc(sizeof(Expr));
        expr->type = QUOTE;
        expr->data.apply.arg = quoted_expr;
        return expr;
    } else if (strcmp(token, "define") == 0) {
        free(token);
        char *var = read_token(input);
        Expr *value = parse_expr(input);
        free(read_token(input));  // consume closing parenthesis
        Expr *expr = malloc(sizeof(Expr));
        expr->type = DEFINE;
        expr->data.apply.func = make_var(var);  // Store variable name
        expr->data.apply.arg = value;  // Store value
        return expr;
    } else {
        Expr *func = make_var(token);
        Expr *arg = parse_expr(input);
        free(read_token(input));  // consume closing parenthesis
        return make_apply(func, arg);
    }
}

Expr *parse_expr(char **input) {
    char *token = read_token(input);
    if (token[0] == '(') {
        free(token);
        return parse_list(input);
    } else if (isdigit(token[0])) {
        int value = atoi(token);
        free(token);
        return make_int(value);
    } else {
        Expr *var = make_var(token);
        free(token);
        return var;
    }
}

void repl() {
    char input[256];
    Environment *env = NULL;

    while (1) {
        printf("> ");
        if (!fgets(input, sizeof(input), stdin)) break;
        char *p = input;
        Expr *expr = parse_expr(&p);
        Expr *result = eval(expr, env);
        if (result->type == INT_LITERAL) {
            printf("%d\n", result->data.int_value);
        } else {
            printf("Expression evaluated.\n");
        }
        // Free memory allocated for the expression (not implemented here)
    }
}

int main() {
    repl();
    return 0;
}

