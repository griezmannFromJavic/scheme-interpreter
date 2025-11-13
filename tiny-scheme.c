/*
 * Minimal Scheme-like interpreter in C.
 * Compile: gcc -std=c99 -O2 -o tiny_scheme tiny_scheme.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* --- Value representation --------------------------------------------------*/

typedef enum {T_NIL, T_NUMBER, T_SYMBOL, T_CONS, T_PROC} Type;

typedef struct Value Value;
typedef struct Env Env;

typedef Value* (*PrimFn)(Value* args, Env* env);

struct Value {
	Type type;
	union {
		double number;
		char* sym;
		struct {
			Value* car;
			Value* cdr;
		} cons;
		struct {
			Value* params;
			Value* body;
			Env* env;
			int is_primitive;
			PrimFn prim;
		} proc;
	} v;
};

struct Env {
	char* sym;
	Value* val;
	Env* next;
	Env* parent;
};

/* --- Helpers ---------------------------------------------------------------*/

Value* mk_nil();
Value* mk_number(double x);
Value* mk_symbol(const char *s);
Value* mk_cons(Value* a, Value* d);
Value* mk_proc(Value* params, Value* body, Env* env);
Value* mk_prim(PrimFn f);

void print_val(Value* v);

/* --- Memory helpers (very small, no GC) ----------------------------------*/

void* smalloc(size_t n) {
	void *p = malloc(n);
	if (!p) {
		perror("malloc");
		exit(1);
	}
	return p;
}
char* sdup(const char *s) {
	char *r = smalloc(strlen(s) + 1);
	strcpy(r, s);
	return r;
}

/* --- Constructors ---------------------------------------------------------*/
Value* mk_nil() {
	Value* v = smalloc(sizeof(Value));
	v->type = T_NIL;
	return v;
}
Value* mk_number(double x) {
	Value* v = smalloc(sizeof(Value));
	v->type = T_NUMBER;
	v->v.number = x;
	return v;
}
Value* mk_symbol(const char *s) {
	Value* v = smalloc(sizeof(Value));
	v->type = T_SYMBOL;
	v->v.sym = sdup(s);
	return v;
}
Value* mk_cons(Value* a, Value* d) {
	Value* v = smalloc(sizeof(Value));
	v->type = T_CONS;
	v->v.cons.car = a;
	v->v.cons.cdr = d;
	return v;
}
Value* mk_proc(Value* params, Value* body, Env* env) {
	Value* v = smalloc(sizeof(Value));
	v->type = T_PROC;
	v->v.proc.params = params;
	v->v.proc.body = body;
	v->v.proc.env = env;
	v->v.proc.is_primitive = 0;
	v->v.proc.prim = NULL;
	return v;
}
Value* mk_prim(PrimFn f) {
	Value* v = smalloc(sizeof(Value));
	v->type = T_PROC;
	v->v.proc.is_primitive = 1;
	v->v.proc.prim = f;
	return v;
}

/* --- Environment ----------------------------------------------------------*/

Env* env_new(Env* parent) {
	Env* e = smalloc(sizeof(Env));
	e->sym = NULL;
	e->val = NULL;
	e->next = NULL;
	e->parent = parent;
	return e;
}

void env_define(Env* env, const char* sym, Value* val) {
	Env* node = smalloc(sizeof(Env));
	node->sym = sdup(sym);
	node->val = val;
	node->next = env->next;
	node->parent = env->parent; // keep same parent pointer
	env->next = node;
}

Value* env_lookup(Env* env, const char* sym) {
	for(Env* e = env->next; e; e = e->next)
		if(strcmp(e->sym,sym)==0)
			return e->val;
	if(env->parent)
		return env_lookup(env->parent, sym);
	return NULL;
}

/* --- Reader (tokenize + parse) -------------------------------------------*/

// Simple tokenizer: returns next token in buffer (caller frees token)
char *next_token(char **s) {
    char *p = *s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p)
        return NULL;

	// parentheses handling
	if (*p == '(' || *p == ')') {
        char *t = smalloc(2);
        t[0] = *p;
        t[1] = '\0';
        p++;
        *s = p;
        return t;
    }

    // handle #t and #f explicitly
    if (*p == '#' && (p[1] == 't' || p[1] == 'f')) {
        char *t = smalloc(3);
        t[0] = '#';
        t[1] = p[1];
        t[2] = '\0';
        p += 2;
        *s = p;
        return t;
    }

    // normal symbol/number
    char *start = p;
    while (*p && !isspace((unsigned char)*p) && *p!='(' && *p!=')')
        p++;
    int len = p - start;
    char *t = smalloc(len + 1);
    memcpy(t, start, len);
    t[len] = '\0';
    *s = p;
    return t;
}



// Forward
Value* read_from_tokens(char **s);

Value* read_list(char **s) {
	// assume current pos is after '('
	while(**s && isspace((unsigned char)**s)) (*s)++;
	if (**s==')') {
		(*s)++;
		return mk_nil();
	}
	Value* first = read_from_tokens(s);
	Value* rest = read_list(s);
	return mk_cons(first, rest);
}

int is_number_token(const char *t) {
	char *p = (char*)t;
	if (*p == '+' || *p == '-') p++;
	int has_digit = 0, has_dot = 0;
	while (*p) {
		if(isdigit((unsigned char)*p))
			has_digit = 1;
		else if (*p == '.' && !has_dot)
			has_dot = 1;
		else
			return 0;
		p++;
	}
	return has_digit;
}

Value* read_from_tokens(char **s) {
	char *tok = next_token(s);
	if (!tok) return NULL;
	if (strcmp(tok, "(") == 0) {
		free(tok);
		Value* lst = read_list(s);
		return lst;
	}
	if (strcmp(tok, ")") == 0) {
		free(tok);
		return mk_nil();
	}
	// number?
	if (strcmp(tok, "#t") == 0) {
        free(tok);
        return mk_symbol("#t");
    }
    if (strcmp(tok, "#f") == 0) {
        free(tok);
        return mk_nil();
    }
	if (is_number_token(tok)) {
		double x = atof(tok);
		free(tok);
		return mk_number(x);
	}
	// else symbol
	Value* sym = mk_symbol(tok);
	free(tok);
	return sym;
}

Value* parse (const char* input) {
	char *buf = sdup(input);
	char *p = buf;
	Value* v = read_from_tokens(&p);
	free(buf);
	return v;
}

/* --- Printer --------------------------------------------------------------*/

void print_val(Value* v) {
	if (!v) {
		printf("<null>");
		return;
	}
	switch(v->type) {
	case T_NIL:
		printf("()");
		break;
	case T_NUMBER: {
		double n = v->v.number;
		(n == (int)n) ? printf("%d", (int)n) : printf("%g", n);
		break;
	}
	case T_SYMBOL:
		printf("%s", v->v.sym);
		break;
	case T_CONS: {
		printf("(");
		Value* cur = v;
		int first = 1;
		while (cur->type == T_CONS) {
			if (!first)
				printf(" ");
			print_val(cur->v.cons.car);
			first = 0;
			cur = cur->v.cons.cdr;
		}
		if (cur->type != T_NIL) {
			printf(" . ");
			print_val(cur);
		}
		printf(")");
		break;
	}
	case T_PROC:
		if(v->v.proc.is_primitive) printf("<primitive>");
		else printf("<lambda>");
		break;
	}
}

/* --- Utility list helpers -------------------------------------------------*/

int is_nil (Value* v) {
	return v == NULL || v->type == T_NIL;
}
Value* list_n (Value** arr, int n) {
	Value* r = mk_nil();
	for(int i = n - 1; i >= 0; --i)
		r = mk_cons(arr[i], r);
	return r;
}
int list_length (Value* v) {
	int i=0;
	while (v->type==T_CONS) {
		i++;
		v = v->v.cons.cdr;
	}
	return i;
}
Value* list_ref (Value* v, int idx) {
	while(idx-->0)
		v = v->v.cons.cdr;
	return v->v.cons.car;
}

/* --- Eval / Apply --------------------------------------------------------*/

Value* eval (Value* expr, Env* env);
Value* eval_list(Value* list, Env* env) {
	if (is_nil(list))
		return mk_nil();
	Value* first = eval(list->v.cons.car, env);
	return mk_cons(first, eval_list(list->v.cons.cdr, env));
}

Value* apply(Value* proc, Value* args, Env* env) {
	if (proc->type!=T_PROC) {
		printf("Attempt to apply non-procedure\n");
		return mk_nil();
	}
	if (proc->v.proc.is_primitive)
		return proc->v.proc.prim(args, env);
	// create new env frame
	Env* newenv = env_new(proc->v.proc.env);
	// bind params
	Value* params = proc->v.proc.params;
	Value* cur = args;
	while (params->type==T_CONS) {
		if(cur->type!=T_CONS) {
			printf("wrong number of args\n");
			return mk_nil();
		}
		Value* sym = params->v.cons.car;
		if (sym->type != T_SYMBOL) {
			printf("param not symbol\n");
			return mk_nil();
		}
		env_define(newenv, sym->v.sym, cur->v.cons.car);
		params = params->v.cons.cdr;
		cur = cur->v.cons.cdr;
	}
	// evaluate body (single expression since we don't have begin)
	return eval(proc->v.proc.body, newenv);
}

int is_symbol(Value* v, const char* s) {
	return v && v->type == T_SYMBOL && strcmp(v->v.sym,s) == 0;
}

Value* eval(Value* expr, Env* env) {
	if (expr==NULL)
		return mk_nil();
	switch(expr->type) {
	case T_NIL:
		return expr;
	case T_NUMBER:
		return expr;
	case T_SYMBOL: {
		Value* val = env_lookup(env, expr->v.sym);
		if (!val) {
			printf("Unbound symbol: %s\n", expr->v.sym);
			return mk_nil();
		}
		return val;
	}
	case T_CONS: {
		Value* op = expr->v.cons.car;
		Value* args = expr->v.cons.cdr;
		// special forms
		if (is_symbol(op, "quote")) {
			return args->v.cons.car; // (quote x) -> x
		}
		if (is_symbol(op, "if")) {
			Value* test = eval(args->v.cons.car, env);
			Value* conseq = args->v.cons.cdr->v.cons.car;
			Value* alt = args->v.cons.cdr->v.cons.cdr->v.cons.car;
			int cond = !(test->type==T_NIL);
			return eval(cond ? conseq : alt, env);
		}
		if (is_symbol(op, "define")) {
			Value* sym = args->v.cons.car;
			Value* val_expr = args->v.cons.cdr->v.cons.car;
			Value* val = eval(val_expr, env);
			if (sym->type!=T_SYMBOL) {
				printf("define: first arg must be symbol\n");
				return mk_nil();
			}
			// define in current env frame
			env_define(env, sym->v.sym, val);
			return sym;
		}
		if (is_symbol(op, "lambda")) {
			Value* params = args->v.cons.car;
			Value* body = args->v.cons.cdr->v.cons.car; // single expr body (nn)
			return mk_proc(params, body, env);
		}
		// otherwise function call
		Value* proc = eval(op, env);
		Value* evaled_args = eval_list(args, env);
		return apply(proc, evaled_args, env);
	}
	case T_PROC:
		return expr;
	}
	return mk_nil();
}

/* --- Primitives ----------------------------------------------------------*/
Value* prim_nullp(Value* args, Env* env) {
    Value* v = args->v.cons.car;   // take first argument
    return is_nil(v) ? mk_symbol("#t") : mk_nil();
}

Value* prim_arith(Value* args, Env* env, char op) {
	if (is_nil(args))
		return mk_number(0);
	double acc = 0;
	int first = 1;
	while (args->type == T_CONS) {
		Value* a = args->v.cons.car;
		if (a->type!=T_NUMBER) {
			printf("arith: arg not number\n");
			return mk_nil();
		}
		if (first) {
			acc = a->v.number;
			first = 0;
		} else {
			if (op =='+')
				acc += a->v.number;
			if (op=='-')
				acc -= a->v.number;
			if (op=='*')
				acc *= a->v.number;
			if(op=='/')
				acc /= a->v.number;
		}
		args = args->v.cons.cdr;
	}
	return mk_number(acc);
}

Value* prim_plus(Value* args, Env* env) {
	return prim_arith(args, env, '+');
}
Value* prim_minus(Value* args, Env* env) {
	return prim_arith(args, env, '-');
}
Value* prim_mul (Value* args, Env* env) {
	return prim_arith(args, env, '*');
}
Value* prim_div(Value* args, Env* env) {
	return prim_arith(args, env, '/');
}

Value* prim_numcmp(Value* args, Env* env, char *op) {
	Value* a = args->v.cons.car;
	Value* b = args->v.cons.cdr->v.cons.car;
	if (a->type != T_NUMBER || b->type != T_NUMBER) {
		printf("cmp: args must be numbers\n");
		return mk_nil();
	}
	int res = 0;
	if (strcmp(op, "=") == 0)
		res = (a->v.number == b->v.number);
	if (strcmp(op, "<") == 0)
		res = (a->v.number<b->v.number);
	if(strcmp(op, ">") == 0)
		res = (a->v.number>b->v.number);
	return res ? mk_symbol("#t") : mk_nil();
}
Value* prim_eq (Value* args, Env* env) {
	return prim_numcmp(args,env,"=");
}
Value* prim_lt (Value* args, Env* env) {
	return prim_numcmp(args,env,"<");
}
Value* prim_gt(Value* args, Env* env) {
	return prim_numcmp(args, env, ">");
}

Value* prim_cons(Value* args, Env* env) {
	Value* a = args->v.cons.car;
	Value* d = args->v.cons.cdr->v.cons.car;
	return mk_cons(a,d);
}
Value* prim_car(Value* args, Env* env) {
	Value* a = args->v.cons.car;
	if (a->type != T_CONS) {
		printf("car on non-cons\n");
		return mk_nil();
	}
	return a->v.cons.car;
}
Value* prim_cdr(Value* args, Env* env) {
	Value* a = args->v.cons.car;
	if(a->type!=T_CONS) {
		printf("cdr on non-cons\n");
		return mk_nil();
	}
	return a->v.cons.cdr;
}

Value* prim_list(Value* args, Env* env) {
	return args;
}
Value* prim_display(Value* args, Env* env) {
	print_val(args->v.cons.car);
	printf("\n");
	return mk_nil();
}

Value* prim_eval(Value* args, Env* env) {
	Value* form = args->v.cons.car;
	return eval(form, env);
}

Value* prim_load(Value* args, Env* env) {
    Value* arg = args->v.cons.car;
    if (arg->type != T_SYMBOL) {
        printf("load: expected symbol as filename (e.g. (load example.scm))\n");
        return mk_nil();
    }

    const char* filename = arg->v.sym;
    FILE* f = fopen(filename, "r");
    if (!f) {
        perror("load: cannot open file");
        return mk_nil();
    }

    // Read entire file into a buffer
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    char* buf = smalloc(len + 1);
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);

    // Evaluate all expressions in the file sequentially
    char* p = buf;
    Value* last = mk_nil();
    while (1) {
        Value* expr = read_from_tokens(&p);
        if (!expr) break;
        last = eval(expr, env);
    }

    free(buf);
    return last;  // return last evaluated expression
}


/* --- Bootstrap global env -----------------------------------------------*/

Env* make_global() {
	Env* g = env_new(NULL);
	env_define(g, "+", mk_prim(prim_plus));
	env_define(g, "-", mk_prim(prim_minus));
	env_define(g, "*", mk_prim(prim_mul));
	env_define(g, "/", mk_prim(prim_div));
	env_define(g, "=", mk_prim(prim_eq));
	env_define(g, "<", mk_prim(prim_lt));
	env_define(g, ">", mk_prim(prim_gt));
	env_define(g, "cons", mk_prim(prim_cons));
	env_define(g, "car", mk_prim(prim_car));
	env_define(g, "cdr", mk_prim(prim_cdr));
	env_define(g, "list", mk_prim(prim_list));
	env_define(g, "display", mk_prim(prim_display));
	env_define(g, "eval", mk_prim(prim_eval));
	env_define(g, "null?", mk_prim(prim_nullp));
	env_define(g, "#t", mk_symbol("#t"));
	env_define(g, "load", mk_prim(prim_load));

	return g;
}

/* --- REPL ----------------------------------------------------------------*/

int main(void) {
    Env *global_env = make_global();
    char buf[4096];
    char line[512];

    printf("tiny-scheme interpreter Ctrl-D to exit.\n");

    while (1) {
        printf("scheme> ");
        buf[0] = '\0';
        int open = 0, close = 0;

        while (fgets(line, sizeof line, stdin)) {
            strcat(buf, line);
            for (char *p = line; *p; p++) {
                if (*p == '(') open++;
                else if (*p == ')') close++;
            }
            if ((open == 0 && strlen(buf) > 0) || (open > 0 && open == close))
                break;  // complete expression
            printf("... ");  // continuation prompt
        }

        if (feof(stdin))
            break;  // Ctrl-D exits

        Value *expr = parse(buf);
        Value *result = eval(expr, global_env);
        print_val(result);
        printf("\n");
    }

    return 0;
}
