/*
 * compiler.c
 *
 * A teaching-oriented Rust subset compiler implemented in C.
 *
 * Design goals:
 * - Keep the code readable and heavily commented for compiler education.
 * - Support a practical subset of Rust syntax and semantics.
 * - Compile the source program into a simple stack-based bytecode.
 * - Execute that bytecode in a small virtual machine.
 *
 * Supported language features:
 * - Integer and boolean literals.
 * - Variables with immutable and mutable bindings.
 * - Arithmetic, comparison, and boolean operators.
 * - if / else expressions.
 * - while expressions.
 * - Blocks and block expressions.
 * - Function definitions and function calls.
 * - return statements.
 *
 * Unsupported Rust features (intentional for a teaching compiler):
 * - Lifetimes, ownership, traits, structs, enums, generics, modules,
 *   pattern matching, arrays, references, and the borrow checker.
 *
 * The implementation focuses on a rigorous, well-structured subset that is
 * suitable for compiler-principles teaching, not on full Rust compatibility.
 */

#define _CRT_SECURE_NO_WARNINGS

#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"

/* ------------------------------------------------------------------------- */
/* Utility helpers                                                           */
/* ------------------------------------------------------------------------- */

static void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    return ptr;
}

static void *xrealloc(void *ptr, size_t size) {
    void *result = realloc(ptr, size);
    if (!result) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    return result;
}

static char *str_dup_range(const char *start, size_t length) {
    char *text = (char *)xmalloc(length + 1);
    memcpy(text, start, length);
    text[length] = '\0';
    return text;
}

static char *str_dup_c(const char *text) {
    return str_dup_range(text, strlen(text));
}

static void fatal_impl(const char *file, int line, const char *fmt, va_list ap) {
    fprintf(stderr, "error: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n  at %s:%d\n", file, line);
    exit(1);
}

static void fatal_at(const char *file, int line, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fatal_impl(file, line, fmt, ap);
    va_end(ap);
}

#define FATAL(...) fatal_at(__FILE__, __LINE__, __VA_ARGS__)

/* ------------------------------------------------------------------------- */
/* Types                                                                     */
/* ------------------------------------------------------------------------- */

typedef enum {
    TYPE_INVALID,
    TYPE_UNIT,
    TYPE_I64,
    TYPE_BOOL,
} TypeKind;

typedef struct {
    TypeKind kind;
} Type;

static Type type_invalid(void) { return (Type){ TYPE_INVALID }; }
static Type type_unit(void) { return (Type){ TYPE_UNIT }; }
static Type type_i64(void) { return (Type){ TYPE_I64 }; }
static Type type_bool(void) { return (Type){ TYPE_BOOL }; }

static bool type_equals(Type a, Type b) {
    return a.kind == b.kind;
}

static const char *type_name(Type t) {
    switch (t.kind) {
    case TYPE_UNIT: return "()";
    case TYPE_I64: return "i64";
    case TYPE_BOOL: return "bool";
    default: return "<invalid>";
    }
}

/* ------------------------------------------------------------------------- */
/* Tokens                                                                    */
/* ------------------------------------------------------------------------- */

typedef enum {
    TOK_EOF,
    TOK_IDENT,
    TOK_INT,
    TOK_FN,
    TOK_LET,
    TOK_MUT,
    TOK_RETURN,
    TOK_BREAK,
    TOK_CONTINUE,
    TOK_IF,
    TOK_ELSE,
    TOK_WHILE,
    TOK_TRUE,
    TOK_FALSE,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_COMMA,
    TOK_COLON,
    TOK_SEMICOLON,
    TOK_ARROW,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_PERCENT,
    TOK_BANG,
    TOK_EQ,
    TOK_EQEQ,
    TOK_NEQ,
    TOK_LT,
    TOK_LE,
    TOK_GT,
    TOK_GE,
    TOK_ANDAND,
    TOK_OROR,
    TOK_ERROR,
} TokenKind;

typedef struct {
    TokenKind kind;
    char *text;
    long long int_value;
    int line;
    int col;
} Token;

typedef struct {
    const char *src;
    size_t pos;
    int line;
    int col;
} Lexer;

static char lexer_peek(Lexer *lex) {
    return lex->src[lex->pos];
}

static char lexer_peek_next(Lexer *lex) {
    if (lex->src[lex->pos] == '\0') {
        return '\0';
    }
    return lex->src[lex->pos + 1];
}

static char lexer_advance(Lexer *lex) {
    char ch = lex->src[lex->pos];
    if (ch == '\0') {
        return '\0';
    }
    lex->pos++;
    if (ch == '\n') {
        lex->line++;
        lex->col = 1;
    } else {
        lex->col++;
    }
    return ch;
}

static void lexer_skip_ws_and_comments(Lexer *lex) {
    for (;;) {
        char ch = lexer_peek(lex);
        if (isspace((unsigned char)ch)) {
            lexer_advance(lex);
            continue;
        }
        if (ch == '/' && lexer_peek_next(lex) == '/') {
            while (lexer_peek(lex) != '\0' && lexer_peek(lex) != '\n') {
                lexer_advance(lex);
            }
            continue;
        }
        if (ch == '/' && lexer_peek_next(lex) == '*') {
            lexer_advance(lex);
            lexer_advance(lex);
            while (lexer_peek(lex) != '\0') {
                if (lexer_peek(lex) == '*' && lexer_peek_next(lex) == '/') {
                    lexer_advance(lex);
                    lexer_advance(lex);
                    break;
                }
                lexer_advance(lex);
            }
            continue;
        }
        break;
    }
}

static Token token_make(TokenKind kind, int line, int col) {
    Token token;
    token.kind = kind;
    token.text = NULL;
    token.int_value = 0;
    token.line = line;
    token.col = col;
    return token;
}

static Token lex_next(Lexer *lex) {
    lexer_skip_ws_and_comments(lex);

    int line = lex->line;
    int col = lex->col;
    char ch = lexer_peek(lex);
    if (ch == '\0') {
        return token_make(TOK_EOF, line, col);
    }

    /* Identifiers and keywords. */
    if (isalpha((unsigned char)ch) || ch == '_') {
        size_t start = lex->pos;
        lexer_advance(lex);
        while (isalnum((unsigned char)lexer_peek(lex)) || lexer_peek(lex) == '_') {
            lexer_advance(lex);
        }
        size_t length = lex->pos - start;
        char *text = str_dup_range(lex->src + start, length);
        Token token = token_make(TOK_IDENT, line, col);
        token.text = text;

        if (strcmp(text, "fn") == 0) token.kind = TOK_FN;
        else if (strcmp(text, "let") == 0) token.kind = TOK_LET;
        else if (strcmp(text, "mut") == 0) token.kind = TOK_MUT;
        else if (strcmp(text, "return") == 0) token.kind = TOK_RETURN;
        else if (strcmp(text, "break") == 0) token.kind = TOK_BREAK;
        else if (strcmp(text, "continue") == 0) token.kind = TOK_CONTINUE;
        else if (strcmp(text, "if") == 0) token.kind = TOK_IF;
        else if (strcmp(text, "else") == 0) token.kind = TOK_ELSE;
        else if (strcmp(text, "while") == 0) token.kind = TOK_WHILE;
        else if (strcmp(text, "true") == 0) token.kind = TOK_TRUE;
        else if (strcmp(text, "false") == 0) token.kind = TOK_FALSE;

        return token;
    }

    /* Numbers are parsed as decimal i64 values. */
    if (isdigit((unsigned char)ch)) {
        long long value = 0;
        while (isdigit((unsigned char)lexer_peek(lex))) {
            value = value * 10 + (lexer_advance(lex) - '0');
        }
        Token token = token_make(TOK_INT, line, col);
        token.int_value = value;
        return token;
    }

    /* Multi-character operators are checked first. */
    if (ch == '-' && lexer_peek_next(lex) == '>') {
        lexer_advance(lex);
        lexer_advance(lex);
        return token_make(TOK_ARROW, line, col);
    }
    if (ch == '=' && lexer_peek_next(lex) == '=') {
        lexer_advance(lex);
        lexer_advance(lex);
        return token_make(TOK_EQEQ, line, col);
    }
    if (ch == '!' && lexer_peek_next(lex) == '=') {
        lexer_advance(lex);
        lexer_advance(lex);
        return token_make(TOK_NEQ, line, col);
    }
    if (ch == '<' && lexer_peek_next(lex) == '=') {
        lexer_advance(lex);
        lexer_advance(lex);
        return token_make(TOK_LE, line, col);
    }
    if (ch == '>' && lexer_peek_next(lex) == '=') {
        lexer_advance(lex);
        lexer_advance(lex);
        return token_make(TOK_GE, line, col);
    }
    if (ch == '&' && lexer_peek_next(lex) == '&') {
        lexer_advance(lex);
        lexer_advance(lex);
        return token_make(TOK_ANDAND, line, col);
    }
    if (ch == '|' && lexer_peek_next(lex) == '|') {
        lexer_advance(lex);
        lexer_advance(lex);
        return token_make(TOK_OROR, line, col);
    }

    /* Single-character punctuation and operators. */
    lexer_advance(lex);
    switch (ch) {
    case '(': return token_make(TOK_LPAREN, line, col);
    case ')': return token_make(TOK_RPAREN, line, col);
    case '{': return token_make(TOK_LBRACE, line, col);
    case '}': return token_make(TOK_RBRACE, line, col);
    case ',': return token_make(TOK_COMMA, line, col);
    case ':': return token_make(TOK_COLON, line, col);
    case ';': return token_make(TOK_SEMICOLON, line, col);
    case '+': return token_make(TOK_PLUS, line, col);
    case '-': return token_make(TOK_MINUS, line, col);
    case '*': return token_make(TOK_STAR, line, col);
    case '/': return token_make(TOK_SLASH, line, col);
    case '%': return token_make(TOK_PERCENT, line, col);
    case '!': return token_make(TOK_BANG, line, col);
    case '=': return token_make(TOK_EQ, line, col);
    case '<': return token_make(TOK_LT, line, col);
    case '>': return token_make(TOK_GT, line, col);
    default:
        FATAL("unexpected character '%c' at %d:%d", ch, line, col);
    }

    return token_make(TOK_ERROR, line, col);
}

/* ------------------------------------------------------------------------- */
/* AST                                                                        */
/* ------------------------------------------------------------------------- */

typedef struct Expr Expr;
typedef struct Stmt Stmt;
typedef struct Block Block;
typedef struct Param Param;
typedef struct Function Function;
typedef struct Program Program;

typedef enum {
    EXPR_INT,
    EXPR_BOOL,
    EXPR_UNIT,
    EXPR_VAR,
    EXPR_UNARY,
    EXPR_BINARY,
    EXPR_ASSIGN,
    EXPR_CALL,
    EXPR_IF,
    EXPR_WHILE,
    EXPR_BLOCK,
} ExprKind;

typedef enum {
    UNARY_NEG,
    UNARY_NOT,
} UnaryKind;

typedef enum {
    BIN_ADD,
    BIN_SUB,
    BIN_MUL,
    BIN_DIV,
    BIN_MOD,
    BIN_EQ,
    BIN_NE,
    BIN_LT,
    BIN_LE,
    BIN_GT,
    BIN_GE,
    BIN_AND,
    BIN_OR,
} BinaryKind;

typedef enum {
    STMT_LET,
    STMT_RETURN,
    STMT_BREAK,
    STMT_CONTINUE,
    STMT_EXPR,
} StmtKind;

struct Expr {
    ExprKind kind;
    Type type;
    int line;
    int col;
    union {
        long long int_value;
        bool bool_value;
        struct {
            char *name;
            int slot;
        } var;
        struct {
            UnaryKind op;
            Expr *operand;
        } unary;
        struct {
            BinaryKind op;
            Expr *left;
            Expr *right;
        } binary;
        struct {
            char *name;
            Expr *value;
            int slot;
            bool mut;
        } assign;
        struct {
            char *name;
            Expr **args;
            size_t argc;
            int fn_index;
        } call;
        struct {
            Expr *cond;
            Block *then_block;
            Block *else_block;
            bool has_else;
        } if_expr;
        struct {
            Expr *cond;
            Block *body;
        } while_expr;
        struct {
            Block *block;
        } block_expr;
    } as;
};

struct Stmt {
    StmtKind kind;
    int line;
    int col;
    union {
        struct {
            bool mut;
            char *name;
            bool has_annotation;
            Type annotation;
            Expr *init;
            int slot;
            Type type;
        } let_stmt;
        struct {
            bool has_expr;
            Expr *expr;
        } return_stmt;
        struct {
            Expr *expr;
        } expr_stmt;
    } as;
};

struct Block {
    Stmt **stmts;
    size_t stmt_count;
    size_t stmt_cap;
    Expr *tail;
    int line;
    int col;
};

struct Param {
    char *name;
    Type type;
    int slot;
};

struct Function {
    char *name;
    Param *params;
    size_t param_count;
    size_t param_cap;
    Type return_type;
    Block *body;
    int index;
    int local_count;
    int line;
    int col;
};

struct Program {
    Function **functions;
    size_t function_count;
    size_t function_cap;
};

static Expr *expr_new(ExprKind kind, int line, int col) {
    Expr *expr = (Expr *)xmalloc(sizeof(Expr));
    memset(expr, 0, sizeof(Expr));
    expr->kind = kind;
    expr->type = type_invalid();
    expr->line = line;
    expr->col = col;
    return expr;
}

static Stmt *stmt_new(StmtKind kind, int line, int col) {
    Stmt *stmt = (Stmt *)xmalloc(sizeof(Stmt));
    memset(stmt, 0, sizeof(Stmt));
    stmt->kind = kind;
    stmt->line = line;
    stmt->col = col;
    return stmt;
}

static Block *block_new(int line, int col) {
    Block *block = (Block *)xmalloc(sizeof(Block));
    memset(block, 0, sizeof(Block));
    block->line = line;
    block->col = col;
    return block;
}

static Function *function_new(int line, int col) {
    Function *fn = (Function *)xmalloc(sizeof(Function));
    memset(fn, 0, sizeof(Function));
    fn->return_type = type_unit();
    fn->line = line;
    fn->col = col;
    return fn;
}

static Program *program_new(void) {
    Program *program = (Program *)xmalloc(sizeof(Program));
    memset(program, 0, sizeof(Program));
    return program;
}

static void block_push_stmt(Block *block, Stmt *stmt) {
    if (block->stmt_count == block->stmt_cap) {
        size_t new_cap = block->stmt_cap == 0 ? 8 : block->stmt_cap * 2;
        block->stmts = (Stmt **)xrealloc(block->stmts, new_cap * sizeof(Stmt *));
        block->stmt_cap = new_cap;
    }
    block->stmts[block->stmt_count++] = stmt;
}

static void function_push_param(Function *fn, Param param) {
    if (fn->param_count == fn->param_cap) {
        size_t new_cap = fn->param_cap == 0 ? 8 : fn->param_cap * 2;
        fn->params = (Param *)xrealloc(fn->params, new_cap * sizeof(Param));
        fn->param_cap = new_cap;
    }
    fn->params[fn->param_count++] = param;
}

static void program_push_function(Program *program, Function *fn) {
    if (program->function_count == program->function_cap) {
        size_t new_cap = program->function_cap == 0 ? 8 : program->function_cap * 2;
        program->functions = (Function **)xrealloc(program->functions, new_cap * sizeof(Function *));
        program->function_cap = new_cap;
    }
    program->functions[program->function_count++] = fn;
}

/* ------------------------------------------------------------------------- */
/* Parser                                                                     */
/* ------------------------------------------------------------------------- */

typedef struct {
    Lexer lex;
    Token current;
    Token previous;
} Parser;

static void token_free(Token *tok) {
    free(tok->text);
    tok->text = NULL;
}

static void parser_advance(Parser *p) {
    token_free(&p->previous);
    p->previous = p->current;
    p->current = lex_next(&p->lex);
}

static bool parser_match(Parser *p, TokenKind kind) {
    if (p->current.kind != kind) {
        return false;
    }
    parser_advance(p);
    return true;
}

static void parser_expect(Parser *p, TokenKind kind, const char *message) {
    if (p->current.kind != kind) {
        FATAL("%s at %d:%d", message, p->current.line, p->current.col);
    }
    parser_advance(p);
}

static bool token_is_ident(const Token *tok, const char *name) {
    return tok->kind == TOK_IDENT && tok->text && strcmp(tok->text, name) == 0;
}

static Type parse_type(Parser *p) {
    if (token_is_ident(&p->current, "i64")) {
        parser_advance(p);
        return type_i64();
    }
    if (token_is_ident(&p->current, "bool")) {
        parser_advance(p);
        return type_bool();
    }
    if (parser_match(p, TOK_LPAREN)) {
        parser_expect(p, TOK_RPAREN, "expected ')' after '('");
        return type_unit();
    }
    FATAL("expected type name at %d:%d", p->current.line, p->current.col);
    return type_invalid();
}

static Expr *parse_expr(Parser *p);
static Block *parse_block(Parser *p);

static Expr *parse_primary(Parser *p) {
    Token tok = p->current;

    if (parser_match(p, TOK_INT)) {
        Expr *expr = expr_new(EXPR_INT, tok.line, tok.col);
        expr->as.int_value = tok.int_value;
        expr->type = type_i64();
        return expr;
    }
    if (parser_match(p, TOK_TRUE)) {
        Expr *expr = expr_new(EXPR_BOOL, tok.line, tok.col);
        expr->as.bool_value = true;
        expr->type = type_bool();
        return expr;
    }
    if (parser_match(p, TOK_FALSE)) {
        Expr *expr = expr_new(EXPR_BOOL, tok.line, tok.col);
        expr->as.bool_value = false;
        expr->type = type_bool();
        return expr;
    }
    if (parser_match(p, TOK_LPAREN)) {
        if (parser_match(p, TOK_RPAREN)) {
            Expr *expr = expr_new(EXPR_UNIT, tok.line, tok.col);
            expr->type = type_unit();
            return expr;
        }
        Expr *expr = parse_expr(p);
        parser_expect(p, TOK_RPAREN, "expected ')' after expression");
        return expr;
    }
    if (p->current.kind == TOK_LBRACE) {
        /* A block is a primary expression in Rust, so parse it and wrap it
         * in the dedicated AST node instead of treating it like punctuation. */
        Block *block = parse_block(p);
        Expr *expr = expr_new(EXPR_BLOCK, tok.line, tok.col);
        expr->as.block_expr.block = block;
        expr->type = type_invalid();
        return expr;
    }
    if (parser_match(p, TOK_IF)) {
        Expr *expr = expr_new(EXPR_IF, tok.line, tok.col);
        expr->as.if_expr.cond = parse_expr(p);
        expr->as.if_expr.then_block = parse_block(p);
        if (parser_match(p, TOK_ELSE)) {
            expr->as.if_expr.has_else = true;
            if (p->current.kind == TOK_IF) {
                Expr *else_if = parse_primary(p);
                if (else_if->kind != EXPR_IF) {
                    FATAL("internal parser error while reading else-if");
                }
                Block *wrapper = block_new(else_if->line, else_if->col);
                Stmt *stmt = stmt_new(STMT_EXPR, else_if->line, else_if->col);
                stmt->as.expr_stmt.expr = else_if;
                block_push_stmt(wrapper, stmt);
                wrapper->tail = NULL;
                expr->as.if_expr.else_block = wrapper;
            } else {
                expr->as.if_expr.else_block = parse_block(p);
            }
        }
        return expr;
    }
    if (parser_match(p, TOK_WHILE)) {
        Expr *expr = expr_new(EXPR_WHILE, tok.line, tok.col);
        expr->as.while_expr.cond = parse_expr(p);
        expr->as.while_expr.body = parse_block(p);
        return expr;
    }
    if (p->current.kind == TOK_IDENT) {
        char *name = str_dup_c(p->current.text);
        parser_advance(p);
        if (parser_match(p, TOK_LPAREN)) {
            Expr *call = expr_new(EXPR_CALL, tok.line, tok.col);
            call->as.call.name = name;
            call->as.call.args = NULL;
            call->as.call.argc = 0;
            call->as.call.fn_index = -1;
            if (!parser_match(p, TOK_RPAREN)) {
                for (;;) {
                    Expr *arg = parse_expr(p);
                    call->as.call.args = (Expr **)xrealloc(
                        call->as.call.args,
                        (call->as.call.argc + 1) * sizeof(Expr *));
                    call->as.call.args[call->as.call.argc++] = arg;
                    if (parser_match(p, TOK_COMMA)) {
                        continue;
                    }
                    parser_expect(p, TOK_RPAREN, "expected ')' after call arguments");
                    break;
                }
            }
            return call;
        }
        Expr *expr = expr_new(EXPR_VAR, tok.line, tok.col);
        expr->as.var.name = name;
        expr->as.var.slot = -1;
        return expr;
    }

    FATAL("unexpected token at %d:%d", p->current.line, p->current.col);
    return NULL;
}

static Expr *parse_unary(Parser *p) {
    Token tok = p->current;
    if (parser_match(p, TOK_MINUS)) {
        Expr *expr = expr_new(EXPR_UNARY, tok.line, tok.col);
        expr->as.unary.op = UNARY_NEG;
        expr->as.unary.operand = parse_unary(p);
        return expr;
    }
    if (parser_match(p, TOK_BANG)) {
        Expr *expr = expr_new(EXPR_UNARY, tok.line, tok.col);
        expr->as.unary.op = UNARY_NOT;
        expr->as.unary.operand = parse_unary(p);
        return expr;
    }
    return parse_primary(p);
}

static Expr *parse_factor(Parser *p) {
    Expr *expr = parse_unary(p);
    for (;;) {
        Token tok = p->current;
        BinaryKind op;
        if (parser_match(p, TOK_STAR)) op = BIN_MUL;
        else if (parser_match(p, TOK_SLASH)) op = BIN_DIV;
        else if (parser_match(p, TOK_PERCENT)) op = BIN_MOD;
        else break;
        Expr *rhs = parse_unary(p);
        Expr *node = expr_new(EXPR_BINARY, tok.line, tok.col);
        node->as.binary.op = op;
        node->as.binary.left = expr;
        node->as.binary.right = rhs;
        expr = node;
    }
    return expr;
}

static Expr *parse_term(Parser *p) {
    Expr *expr = parse_factor(p);
    for (;;) {
        Token tok = p->current;
        BinaryKind op;
        if (parser_match(p, TOK_PLUS)) op = BIN_ADD;
        else if (parser_match(p, TOK_MINUS)) op = BIN_SUB;
        else break;
        Expr *rhs = parse_factor(p);
        Expr *node = expr_new(EXPR_BINARY, tok.line, tok.col);
        node->as.binary.op = op;
        node->as.binary.left = expr;
        node->as.binary.right = rhs;
        expr = node;
    }
    return expr;
}

static Expr *parse_comparison(Parser *p) {
    Expr *expr = parse_term(p);
    for (;;) {
        Token tok = p->current;
        BinaryKind op;
        if (parser_match(p, TOK_LT)) op = BIN_LT;
        else if (parser_match(p, TOK_LE)) op = BIN_LE;
        else if (parser_match(p, TOK_GT)) op = BIN_GT;
        else if (parser_match(p, TOK_GE)) op = BIN_GE;
        else break;
        Expr *rhs = parse_term(p);
        Expr *node = expr_new(EXPR_BINARY, tok.line, tok.col);
        node->as.binary.op = op;
        node->as.binary.left = expr;
        node->as.binary.right = rhs;
        expr = node;
    }
    return expr;
}

static Expr *parse_equality(Parser *p) {
    Expr *expr = parse_comparison(p);
    for (;;) {
        Token tok = p->current;
        BinaryKind op;
        if (parser_match(p, TOK_EQEQ)) op = BIN_EQ;
        else if (parser_match(p, TOK_NEQ)) op = BIN_NE;
        else break;
        Expr *rhs = parse_comparison(p);
        Expr *node = expr_new(EXPR_BINARY, tok.line, tok.col);
        node->as.binary.op = op;
        node->as.binary.left = expr;
        node->as.binary.right = rhs;
        expr = node;
    }
    return expr;
}

static Expr *parse_and(Parser *p) {
    Expr *expr = parse_equality(p);
    while (parser_match(p, TOK_ANDAND)) {
        Token tok = p->previous;
        Expr *rhs = parse_equality(p);
        Expr *node = expr_new(EXPR_BINARY, tok.line, tok.col);
        node->as.binary.op = BIN_AND;
        node->as.binary.left = expr;
        node->as.binary.right = rhs;
        expr = node;
    }
    return expr;
}

static Expr *parse_or(Parser *p) {
    Expr *expr = parse_and(p);
    while (parser_match(p, TOK_OROR)) {
        Token tok = p->previous;
        Expr *rhs = parse_and(p);
        Expr *node = expr_new(EXPR_BINARY, tok.line, tok.col);
        node->as.binary.op = BIN_OR;
        node->as.binary.left = expr;
        node->as.binary.right = rhs;
        expr = node;
    }
    return expr;
}

static Expr *parse_assignment(Parser *p) {
    Expr *expr = parse_or(p);
    if (parser_match(p, TOK_EQ)) {
        Token eq = p->previous;
        if (expr->kind != EXPR_VAR) {
            FATAL("assignment target must be a variable at %d:%d", eq.line, eq.col);
        }
        Expr *value = parse_assignment(p);
        Expr *node = expr_new(EXPR_ASSIGN, eq.line, eq.col);
        node->as.assign.name = expr->as.var.name;
        node->as.assign.value = value;
        node->as.assign.slot = -1;
        node->as.assign.mut = false;
        free(expr);
        return node;
    }
    return expr;
}

static Expr *parse_expr(Parser *p) {
    return parse_assignment(p);
}

static Stmt *parse_let_stmt(Parser *p) {
    Token tok = p->current;
    parser_expect(p, TOK_LET, "expected 'let'");
    bool mut = parser_match(p, TOK_MUT);

    if (p->current.kind != TOK_IDENT) {
        FATAL("expected identifier after let at %d:%d", p->current.line, p->current.col);
    }
    char *name = str_dup_c(p->current.text);
    parser_advance(p);

    bool has_annotation = false;
    Type annotation = type_invalid();
    if (parser_match(p, TOK_COLON)) {
        has_annotation = true;
        annotation = parse_type(p);
    }

    parser_expect(p, TOK_EQ, "expected '=' in let binding");
    Expr *init = parse_expr(p);

    Stmt *stmt = stmt_new(STMT_LET, tok.line, tok.col);
    stmt->as.let_stmt.mut = mut;
    stmt->as.let_stmt.name = name;
    stmt->as.let_stmt.has_annotation = has_annotation;
    stmt->as.let_stmt.annotation = annotation;
    stmt->as.let_stmt.init = init;
    stmt->as.let_stmt.slot = -1;
    stmt->as.let_stmt.type = type_invalid();
    return stmt;
}

static Stmt *parse_return_stmt(Parser *p) {
    Token tok = p->current;
    parser_expect(p, TOK_RETURN, "expected 'return'");
    Stmt *stmt = stmt_new(STMT_RETURN, tok.line, tok.col);
    if (p->current.kind != TOK_SEMICOLON) {
        stmt->as.return_stmt.has_expr = true;
        stmt->as.return_stmt.expr = parse_expr(p);
    }
    return stmt;
}

static Stmt *parse_break_stmt(Parser *p) {
    Token tok = p->current;
    parser_expect(p, TOK_BREAK, "expected 'break'");
    return stmt_new(STMT_BREAK, tok.line, tok.col);
}

static Stmt *parse_continue_stmt(Parser *p) {
    Token tok = p->current;
    parser_expect(p, TOK_CONTINUE, "expected 'continue'");
    return stmt_new(STMT_CONTINUE, tok.line, tok.col);
}

static Block *parse_block(Parser *p) {
    Token open = p->current;
    parser_expect(p, TOK_LBRACE, "expected '{'");

    Block *block = block_new(open.line, open.col);
    while (p->current.kind != TOK_RBRACE && p->current.kind != TOK_EOF) {
        if (p->current.kind == TOK_LET) {
            Stmt *stmt = parse_let_stmt(p);
            parser_expect(p, TOK_SEMICOLON, "expected ';' after let binding");
            block_push_stmt(block, stmt);
            continue;
        }
        if (p->current.kind == TOK_RETURN) {
            Stmt *stmt = parse_return_stmt(p);
            parser_expect(p, TOK_SEMICOLON, "expected ';' after return statement");
            block_push_stmt(block, stmt);
            continue;
        }
        if (p->current.kind == TOK_BREAK) {
            Stmt *stmt = parse_break_stmt(p);
            parser_expect(p, TOK_SEMICOLON, "expected ';' after break statement");
            block_push_stmt(block, stmt);
            continue;
        }
        if (p->current.kind == TOK_CONTINUE) {
            Stmt *stmt = parse_continue_stmt(p);
            parser_expect(p, TOK_SEMICOLON, "expected ';' after continue statement");
            block_push_stmt(block, stmt);
            continue;
        }

        Expr *expr = parse_expr(p);
        if (parser_match(p, TOK_SEMICOLON)) {
            Stmt *stmt = stmt_new(STMT_EXPR, expr->line, expr->col);
            stmt->as.expr_stmt.expr = expr;
            block_push_stmt(block, stmt);
            continue;
        }
        if (p->current.kind == TOK_RBRACE) {
            block->tail = expr;
            break;
        }
        FATAL("expected ';' or '}' after expression at %d:%d", p->current.line, p->current.col);
    }

    parser_expect(p, TOK_RBRACE, "expected '}' to close block");
    return block;
}

static Function *parse_function(Parser *p) {
    Token tok = p->current;
    parser_expect(p, TOK_FN, "expected 'fn'");
    if (p->current.kind != TOK_IDENT) {
        FATAL("expected function name after fn at %d:%d", p->current.line, p->current.col);
    }
    char *name = str_dup_c(p->current.text);
    parser_advance(p);

    Function *fn = function_new(tok.line, tok.col);
    fn->name = name;

    parser_expect(p, TOK_LPAREN, "expected '(' after function name");
    if (p->current.kind != TOK_RPAREN) {
        for (;;) {
            if (p->current.kind != TOK_IDENT) {
                FATAL("expected parameter name at %d:%d", p->current.line, p->current.col);
            }
            Param param;
            param.name = str_dup_c(p->current.text);
            param.type = type_invalid();
            param.slot = -1;
            parser_advance(p);
            parser_expect(p, TOK_COLON, "expected ':' after parameter name");
            param.type = parse_type(p);
            function_push_param(fn, param);
            if (parser_match(p, TOK_COMMA)) {
                continue;
            }
            break;
        }
    }
    parser_expect(p, TOK_RPAREN, "expected ')' after function parameters");

    if (parser_match(p, TOK_ARROW)) {
        fn->return_type = parse_type(p);
    } else {
        fn->return_type = type_unit();
    }

    fn->body = parse_block(p);
    return fn;
}

static Program *parse_program(const char *source) {
    Parser p;
    memset(&p, 0, sizeof(p));
    p.lex.src = source;
    p.lex.pos = 0;
    p.lex.line = 1;
    p.lex.col = 1;
    p.current.kind = TOK_ERROR;
    p.previous.kind = TOK_ERROR;
    parser_advance(&p);

    Program *program = program_new();
    while (p.current.kind != TOK_EOF) {
        program_push_function(program, parse_function(&p));
    }
    return program;
}

/* ------------------------------------------------------------------------- */
/* Semantic analysis                                                          */
/* ------------------------------------------------------------------------- */

typedef struct {
    char *name;
    Type type;
    bool mut;
    int slot;
} Symbol;

typedef struct {
    Symbol *items;
    size_t len;
    size_t cap;
} SymbolVec;

static void symbol_vec_init(SymbolVec *vec) {
    vec->items = NULL;
    vec->len = 0;
    vec->cap = 0;
}

static void symbol_vec_push(SymbolVec *vec, Symbol item) {
    if (vec->len == vec->cap) {
        size_t new_cap = vec->cap == 0 ? 8 : vec->cap * 2;
        vec->items = (Symbol *)xrealloc(vec->items, new_cap * sizeof(Symbol));
        vec->cap = new_cap;
    }
    vec->items[vec->len++] = item;
}

typedef struct {
    SymbolVec scopes[64];
    int depth;
    int next_slot;
    int loop_depth;
    Function *current_fn;
    Program *program;
} Sema;

static void sema_push_scope(Sema *s) {
    if (s->depth >= 64) {
        FATAL("scope nesting too deep");
    }
    symbol_vec_init(&s->scopes[s->depth]);
    s->depth++;
}

static void sema_pop_scope(Sema *s) {
    if (s->depth <= 0) {
        FATAL("internal scope underflow");
    }
    s->depth--;
}

static Symbol *sema_lookup(Sema *s, const char *name) {
    for (int depth = s->depth - 1; depth >= 0; --depth) {
        SymbolVec *scope = &s->scopes[depth];
        for (size_t i = scope->len; i > 0; --i) {
            Symbol *sym = &scope->items[i - 1];
            if (strcmp(sym->name, name) == 0) {
                return sym;
            }
        }
    }
    return NULL;
}

static Symbol *sema_lookup_current_scope(Sema *s, const char *name) {
    if (s->depth == 0) {
        return NULL;
    }
    SymbolVec *scope = &s->scopes[s->depth - 1];
    for (size_t i = 0; i < scope->len; ++i) {
        if (strcmp(scope->items[i].name, name) == 0) {
            return &scope->items[i];
        }
    }
    return NULL;
}

static void sema_declare(Sema *s, const char *name, Type type, bool mut, int line, int col) {
    if (sema_lookup_current_scope(s, name)) {
        FATAL("duplicate binding '%s' at %d:%d", name, line, col);
    }
    Symbol sym;
    sym.name = str_dup_c(name);
    sym.type = type;
    sym.mut = mut;
    sym.slot = s->next_slot++;
    symbol_vec_push(&s->scopes[s->depth - 1], sym);
}

static Function *program_find_function(Program *program, const char *name) {
    for (size_t i = 0; i < program->function_count; ++i) {
        if (strcmp(program->functions[i]->name, name) == 0) {
            return program->functions[i];
        }
    }
    return NULL;
}

static void sema_expect_type(Type actual, Type expected, int line, int col, const char *what) {
    if (!type_equals(actual, expected)) {
        FATAL("type mismatch for %s at %d:%d: expected %s but found %s",
              what, line, col, type_name(expected), type_name(actual));
    }
}

static Type sema_check_expr(Sema *s, Expr *expr);
static Type sema_check_block(Sema *s, Block *block);

static Type sema_check_call(Sema *s, Expr *expr) {
    Function *fn = program_find_function(s->program, expr->as.call.name);
    if (!fn) {
        FATAL("unknown function '%s' at %d:%d", expr->as.call.name, expr->line, expr->col);
    }
    if (expr->as.call.argc != fn->param_count) {
        FATAL("function '%s' expects %zu arguments but got %zu at %d:%d",
              expr->as.call.name, fn->param_count, expr->as.call.argc, expr->line, expr->col);
    }
    expr->as.call.fn_index = fn->index;
    for (size_t i = 0; i < expr->as.call.argc; ++i) {
        Type actual = sema_check_expr(s, expr->as.call.args[i]);
        sema_expect_type(actual, fn->params[i].type, expr->as.call.args[i]->line, expr->as.call.args[i]->col, "call argument");
    }
    expr->type = fn->return_type;
    return expr->type;
}

static Type sema_check_binary_arith(Sema *s, Expr *expr, Type expected) {
    Type left = sema_check_expr(s, expr->as.binary.left);
    Type right = sema_check_expr(s, expr->as.binary.right);
    sema_expect_type(left, expected, expr->as.binary.left->line, expr->as.binary.left->col, "left operand");
    sema_expect_type(right, expected, expr->as.binary.right->line, expr->as.binary.right->col, "right operand");
    expr->type = expected;
    return expected;
}

static Type sema_check_expr(Sema *s, Expr *expr) {
    switch (expr->kind) {
    case EXPR_INT:
        expr->type = type_i64();
        return expr->type;
    case EXPR_BOOL:
        expr->type = type_bool();
        return expr->type;
    case EXPR_UNIT:
        expr->type = type_unit();
        return expr->type;
    case EXPR_VAR: {
        Symbol *sym = sema_lookup(s, expr->as.var.name);
        if (!sym) {
            FATAL("unknown variable '%s' at %d:%d", expr->as.var.name, expr->line, expr->col);
        }
        expr->as.var.slot = sym->slot;
        expr->type = sym->type;
        return expr->type;
    }
    case EXPR_UNARY: {
        Type operand = sema_check_expr(s, expr->as.unary.operand);
        if (expr->as.unary.op == UNARY_NEG) {
            sema_expect_type(operand, type_i64(), expr->line, expr->col, "unary -");
            expr->type = type_i64();
        } else {
            sema_expect_type(operand, type_bool(), expr->line, expr->col, "unary !");
            expr->type = type_bool();
        }
        return expr->type;
    }
    case EXPR_BINARY: {
        switch (expr->as.binary.op) {
        case BIN_ADD:
        case BIN_SUB:
        case BIN_MUL:
        case BIN_DIV:
        case BIN_MOD:
            return sema_check_binary_arith(s, expr, type_i64());
        case BIN_EQ:
        case BIN_NE:
            {
                Type left = sema_check_expr(s, expr->as.binary.left);
                Type right = sema_check_expr(s, expr->as.binary.right);
                if (!type_equals(left, right)) {
                    FATAL("comparison type mismatch at %d:%d", expr->line, expr->col);
                }
                expr->type = type_bool();
                return expr->type;
            }
        case BIN_LT:
        case BIN_LE:
        case BIN_GT:
        case BIN_GE:
            return sema_check_binary_arith(s, expr, type_i64()), expr->type = type_bool(), expr->type;
        case BIN_AND:
        case BIN_OR: {
            Type left = sema_check_expr(s, expr->as.binary.left);
            Type right = sema_check_expr(s, expr->as.binary.right);
            sema_expect_type(left, type_bool(), expr->as.binary.left->line, expr->as.binary.left->col, "boolean left operand");
            sema_expect_type(right, type_bool(), expr->as.binary.right->line, expr->as.binary.right->col, "boolean right operand");
            expr->type = type_bool();
            return expr->type;
        }
        }
        break;
    }
    case EXPR_ASSIGN: {
        Symbol *sym = sema_lookup(s, expr->as.assign.name);
        if (!sym) {
            FATAL("unknown variable '%s' at %d:%d", expr->as.assign.name, expr->line, expr->col);
        }
        if (!sym->mut) {
            FATAL("cannot assign to immutable variable '%s' at %d:%d", expr->as.assign.name, expr->line, expr->col);
        }
        Type rhs = sema_check_expr(s, expr->as.assign.value);
        sema_expect_type(rhs, sym->type, expr->as.assign.value->line, expr->as.assign.value->col, "assignment value");
        expr->as.assign.slot = sym->slot;
        expr->as.assign.mut = sym->mut;
        expr->type = type_unit();
        return expr->type;
    }
    case EXPR_CALL:
        return sema_check_call(s, expr);
    case EXPR_IF: {
        Type cond = sema_check_expr(s, expr->as.if_expr.cond);
        sema_expect_type(cond, type_bool(), expr->as.if_expr.cond->line, expr->as.if_expr.cond->col, "if condition");
        Type then_type = sema_check_block(s, expr->as.if_expr.then_block);
        if (expr->as.if_expr.has_else) {
            Type else_type = sema_check_block(s, expr->as.if_expr.else_block);
            if (!type_equals(then_type, else_type)) {
                FATAL("if branches must have the same type at %d:%d", expr->line, expr->col);
            }
            expr->type = then_type;
        } else {
            sema_expect_type(then_type, type_unit(), expr->line, expr->col, "if without else");
            expr->type = type_unit();
        }
        return expr->type;
    }
    case EXPR_WHILE: {
        Type cond = sema_check_expr(s, expr->as.while_expr.cond);
        sema_expect_type(cond, type_bool(), expr->as.while_expr.cond->line, expr->as.while_expr.cond->col, "while condition");
        s->loop_depth++;
        Type body = sema_check_block(s, expr->as.while_expr.body);
        s->loop_depth--;
        sema_expect_type(body, type_unit(), expr->as.while_expr.body->line, expr->as.while_expr.body->col, "while body");
        expr->type = type_unit();
        return expr->type;
    }
    case EXPR_BLOCK:
        expr->type = sema_check_block(s, expr->as.block_expr.block);
        return expr->type;
    }

    FATAL("internal semantic error at %d:%d", expr->line, expr->col);
    return type_invalid();
}

static Type sema_check_stmt(Sema *s, Stmt *stmt, Type function_return_type) {
    switch (stmt->kind) {
    case STMT_LET: {
        Type init_type = sema_check_expr(s, stmt->as.let_stmt.init);
        if (stmt->as.let_stmt.has_annotation) {
            sema_expect_type(init_type, stmt->as.let_stmt.annotation, stmt->line, stmt->col, "let binding");
            stmt->as.let_stmt.type = stmt->as.let_stmt.annotation;
        } else {
            stmt->as.let_stmt.type = init_type;
        }
        sema_declare(s, stmt->as.let_stmt.name, stmt->as.let_stmt.type, stmt->as.let_stmt.mut, stmt->line, stmt->col);
        stmt->as.let_stmt.slot = s->next_slot - 1;
        return type_unit();
    }
    case STMT_RETURN: {
        Type actual = type_unit();
        if (stmt->as.return_stmt.has_expr) {
            actual = sema_check_expr(s, stmt->as.return_stmt.expr);
        }
        sema_expect_type(actual, function_return_type, stmt->line, stmt->col, "return statement");
        return type_unit();
    }
    case STMT_BREAK:
        if (s->loop_depth <= 0) {
            FATAL("break is only valid inside while loops at %d:%d", stmt->line, stmt->col);
        }
        return type_unit();
    case STMT_CONTINUE:
        if (s->loop_depth <= 0) {
            FATAL("continue is only valid inside while loops at %d:%d", stmt->line, stmt->col);
        }
        return type_unit();
    case STMT_EXPR: {
        Type expr_type = sema_check_expr(s, stmt->as.expr_stmt.expr);
        stmt->as.expr_stmt.expr->type = expr_type;
        return type_unit();
    }
    }

    FATAL("internal statement error at %d:%d", stmt->line, stmt->col);
    return type_invalid();
}

static Type sema_check_block(Sema *s, Block *block) {
    sema_push_scope(s);
    for (size_t i = 0; i < block->stmt_count; ++i) {
        sema_check_stmt(s, block->stmts[i], s->current_fn->return_type);
    }
    Type tail_type = type_unit();
    if (block->tail) {
        tail_type = sema_check_expr(s, block->tail);
    }
    sema_pop_scope(s);
    return tail_type;
}

static void sema_check_function(Sema *s, Function *fn) {
    s->current_fn = fn;
    s->next_slot = 0;
    s->depth = 0;

    sema_push_scope(s);
    for (size_t i = 0; i < fn->param_count; ++i) {
        Param *param = &fn->params[i];
        sema_declare(s, param->name, param->type, true, fn->line, fn->col);
        param->slot = s->next_slot - 1;
    }

    Type body_type = sema_check_block(s, fn->body);
    sema_expect_type(body_type, fn->return_type, fn->body->line, fn->body->col, "function body");
    fn->local_count = s->next_slot;

    sema_pop_scope(s);
}

static void sema_check_program(Program *program) {
    /* First build and validate the global function table. */
    for (size_t i = 0; i < program->function_count; ++i) {
        Function *fn = program->functions[i];
        if (program_find_function(program, fn->name) != fn) {
            FATAL("duplicate function '%s' at %d:%d", fn->name, fn->line, fn->col);
        }
        fn->index = (int)i;
    }

    /* Then type-check each function body independently. */
    Sema sema;
    memset(&sema, 0, sizeof(sema));
    sema.program = program;
    for (size_t i = 0; i < program->function_count; ++i) {
        sema_check_function(&sema, program->functions[i]);
    }
}

/* ------------------------------------------------------------------------- */
/* Bytecode and VM                                                            */
/* ------------------------------------------------------------------------- */

typedef enum {
    OP_PUSH_I64,
    OP_LOAD,
    OP_STORE,
    OP_POP,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_NEG,
    OP_EQ,
    OP_NE,
    OP_LT,
    OP_LE,
    OP_GT,
    OP_GE,
    OP_NOT,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_CALL,
    OP_RET,
} OpCode;

typedef struct {
    OpCode op;
    long long arg;
    int aux;
} Instr;

typedef struct {
    Instr *items;
    size_t len;
    size_t cap;
} InstrVec;

static void instr_vec_init(InstrVec *vec) {
    vec->items = NULL;
    vec->len = 0;
    vec->cap = 0;
}

static size_t instr_emit(InstrVec *vec, OpCode op, long long arg, int aux) {
    if (vec->len == vec->cap) {
        size_t new_cap = vec->cap == 0 ? 16 : vec->cap * 2;
        vec->items = (Instr *)xrealloc(vec->items, new_cap * sizeof(Instr));
        vec->cap = new_cap;
    }
    vec->items[vec->len] = (Instr){ op, arg, aux };
    return vec->len++;
}

typedef struct {
    char *name;
    InstrVec code;
    int local_count;
    int param_count;
    Type return_type;
} BytecodeFunction;

typedef struct {
    BytecodeFunction *functions;
    size_t count;
    size_t cap;
} BytecodeProgram;

static void bytecode_program_init(BytecodeProgram *program) {
    program->functions = NULL;
    program->count = 0;
    program->cap = 0;
}

static BytecodeFunction *bytecode_program_push(BytecodeProgram *program, const char *name) {
    if (program->count == program->cap) {
        size_t new_cap = program->cap == 0 ? 8 : program->cap * 2;
        program->functions = (BytecodeFunction *)xrealloc(program->functions, new_cap * sizeof(BytecodeFunction));
        program->cap = new_cap;
    }
    BytecodeFunction *fn = &program->functions[program->count++];
    memset(fn, 0, sizeof(*fn));
    fn->name = str_dup_c(name);
    instr_vec_init(&fn->code);
    return fn;
}

typedef struct {
    Program *ast;
    BytecodeProgram program;
} Codegen;

typedef struct {
    size_t *items;
    size_t len;
    size_t cap;
} IndexVec;

typedef struct LoopContext {
    size_t loop_start;
    IndexVec break_jumps;
    IndexVec continue_jumps;
    struct LoopContext *parent;
} LoopContext;

static void index_vec_push(IndexVec *vec, size_t value) {
    if (vec->len == vec->cap) {
        size_t new_cap = vec->cap == 0 ? 8 : vec->cap * 2;
        vec->items = (size_t *)xrealloc(vec->items, new_cap * sizeof(size_t));
        vec->cap = new_cap;
    }
    vec->items[vec->len++] = value;
}

static size_t codegen_expr(Codegen *cg, Function *fn, BytecodeFunction *out, Expr *expr, LoopContext *loop_ctx);

static size_t codegen_block(Codegen *cg, Function *fn, BytecodeFunction *out, Block *block, LoopContext *loop_ctx) {
    size_t produced = 0;
    for (size_t i = 0; i < block->stmt_count; ++i) {
        Stmt *stmt = block->stmts[i];
        switch (stmt->kind) {
        case STMT_LET:
            codegen_expr(cg, fn, out, stmt->as.let_stmt.init, loop_ctx);
            instr_emit(&out->code, OP_STORE, stmt->as.let_stmt.slot, 0);
            break;
        case STMT_RETURN:
            if (stmt->as.return_stmt.has_expr) {
                codegen_expr(cg, fn, out, stmt->as.return_stmt.expr, loop_ctx);
            } else {
                instr_emit(&out->code, OP_PUSH_I64, 0, 0);
            }
            instr_emit(&out->code, OP_RET, 0, 0);
            break;
        case STMT_BREAK:
            if (!loop_ctx) {
                FATAL("internal codegen error: break outside loop at %d:%d", stmt->line, stmt->col);
            }
            index_vec_push(&loop_ctx->break_jumps, instr_emit(&out->code, OP_JUMP, 0, 0));
            break;
        case STMT_CONTINUE:
            if (!loop_ctx) {
                FATAL("internal codegen error: continue outside loop at %d:%d", stmt->line, stmt->col);
            }
            index_vec_push(&loop_ctx->continue_jumps, instr_emit(&out->code, OP_JUMP, 0, 0));
            break;
        case STMT_EXPR:
            codegen_expr(cg, fn, out, stmt->as.expr_stmt.expr, loop_ctx);
            instr_emit(&out->code, OP_POP, 0, 0);
            break;
        }
    }
    if (block->tail) {
        codegen_expr(cg, fn, out, block->tail, loop_ctx);
        produced = 1;
    } else {
        instr_emit(&out->code, OP_PUSH_I64, 0, 0);
        produced = 1;
    }
    return produced;
}

static size_t codegen_call(Codegen *cg, Function *fn, BytecodeFunction *out, Expr *expr, LoopContext *loop_ctx) {
    (void)cg;
    (void)fn;
    for (size_t i = 0; i < expr->as.call.argc; ++i) {
        codegen_expr(cg, fn, out, expr->as.call.args[i], loop_ctx);
    }
    instr_emit(&out->code, OP_CALL, expr->as.call.fn_index, (int)expr->as.call.argc);
    return 1;
}

static size_t codegen_expr(Codegen *cg, Function *fn, BytecodeFunction *out, Expr *expr, LoopContext *loop_ctx) {
    (void)fn;
    switch (expr->kind) {
    case EXPR_INT:
        instr_emit(&out->code, OP_PUSH_I64, expr->as.int_value, 0);
        return 1;
    case EXPR_BOOL:
        instr_emit(&out->code, OP_PUSH_I64, expr->as.bool_value ? 1 : 0, 0);
        return 1;
    case EXPR_UNIT:
        instr_emit(&out->code, OP_PUSH_I64, 0, 0);
        return 1;
    case EXPR_VAR:
        instr_emit(&out->code, OP_LOAD, expr->as.var.slot, 0);
        return 1;
    case EXPR_UNARY:
        codegen_expr(cg, fn, out, expr->as.unary.operand, loop_ctx);
        instr_emit(&out->code, expr->as.unary.op == UNARY_NEG ? OP_NEG : OP_NOT, 0, 0);
        return 1;
    case EXPR_BINARY: {
        switch (expr->as.binary.op) {
        case BIN_AND: {
            codegen_expr(cg, fn, out, expr->as.binary.left, loop_ctx);
            size_t jump_false = instr_emit(&out->code, OP_JUMP_IF_FALSE, 0, 0);
            codegen_expr(cg, fn, out, expr->as.binary.right, loop_ctx);
            size_t jump_end = instr_emit(&out->code, OP_JUMP, 0, 0);
            size_t false_target = out->code.len;
            instr_emit(&out->code, OP_PUSH_I64, 0, 0);
            size_t end_target = out->code.len;
            out->code.items[jump_false].arg = (long long)false_target;
            out->code.items[jump_end].arg = (long long)end_target;
            break;
        }
        case BIN_OR: {
            codegen_expr(cg, fn, out, expr->as.binary.left, loop_ctx);
            size_t jump_rhs = instr_emit(&out->code, OP_JUMP_IF_FALSE, 0, 0);
            instr_emit(&out->code, OP_PUSH_I64, 1, 0);
            size_t jump_end = instr_emit(&out->code, OP_JUMP, 0, 0);
            size_t rhs_target = out->code.len;
            codegen_expr(cg, fn, out, expr->as.binary.right, loop_ctx);
            size_t end_target = out->code.len;
            out->code.items[jump_rhs].arg = (long long)rhs_target;
            out->code.items[jump_end].arg = (long long)end_target;
            break;
        }
        default:
            codegen_expr(cg, fn, out, expr->as.binary.left, loop_ctx);
            codegen_expr(cg, fn, out, expr->as.binary.right, loop_ctx);
            switch (expr->as.binary.op) {
            case BIN_ADD: instr_emit(&out->code, OP_ADD, 0, 0); break;
            case BIN_SUB: instr_emit(&out->code, OP_SUB, 0, 0); break;
            case BIN_MUL: instr_emit(&out->code, OP_MUL, 0, 0); break;
            case BIN_DIV: instr_emit(&out->code, OP_DIV, 0, 0); break;
            case BIN_MOD: instr_emit(&out->code, OP_MOD, 0, 0); break;
            case BIN_EQ: instr_emit(&out->code, OP_EQ, 0, 0); break;
            case BIN_NE: instr_emit(&out->code, OP_NE, 0, 0); break;
            case BIN_LT: instr_emit(&out->code, OP_LT, 0, 0); break;
            case BIN_LE: instr_emit(&out->code, OP_LE, 0, 0); break;
            case BIN_GT: instr_emit(&out->code, OP_GT, 0, 0); break;
            case BIN_GE: instr_emit(&out->code, OP_GE, 0, 0); break;
            default: break;
            }
            break;
        }
        return 1;
    }
    case EXPR_ASSIGN:
        codegen_expr(cg, fn, out, expr->as.assign.value, loop_ctx);
        instr_emit(&out->code, OP_STORE, expr->as.assign.slot, 0);
        instr_emit(&out->code, OP_PUSH_I64, 0, 0);
        return 1;
    case EXPR_CALL:
        return codegen_call(cg, fn, out, expr, loop_ctx);
    case EXPR_IF: {
        codegen_expr(cg, fn, out, expr->as.if_expr.cond, loop_ctx);
        size_t jump_else = instr_emit(&out->code, OP_JUMP_IF_FALSE, 0, 0);
        codegen_block(cg, fn, out, expr->as.if_expr.then_block, loop_ctx);
        size_t jump_end = instr_emit(&out->code, OP_JUMP, 0, 0);
        size_t else_target = out->code.len;
        if (expr->as.if_expr.has_else) {
            codegen_block(cg, fn, out, expr->as.if_expr.else_block, loop_ctx);
        } else {
            instr_emit(&out->code, OP_PUSH_I64, 0, 0);
        }
        size_t end_target = out->code.len;
        out->code.items[jump_else].arg = (long long)else_target;
        out->code.items[jump_end].arg = (long long)end_target;
        return 1;
    }
    case EXPR_WHILE: {
        size_t loop_start = out->code.len;
        LoopContext loop;
        memset(&loop, 0, sizeof(loop));
        loop.loop_start = loop_start;
        loop.parent = loop_ctx;

        codegen_expr(cg, fn, out, expr->as.while_expr.cond, loop_ctx);
        size_t jump_end = instr_emit(&out->code, OP_JUMP_IF_FALSE, 0, 0);
        codegen_block(cg, fn, out, expr->as.while_expr.body, &loop);
        instr_emit(&out->code, OP_POP, 0, 0);
        instr_emit(&out->code, OP_JUMP, (long long)loop_start, 0);
        size_t end_target = out->code.len;
        out->code.items[jump_end].arg = (long long)end_target;

        for (size_t i = 0; i < loop.continue_jumps.len; ++i) {
            out->code.items[loop.continue_jumps.items[i]].arg = (long long)loop_start;
        }
        for (size_t i = 0; i < loop.break_jumps.len; ++i) {
            out->code.items[loop.break_jumps.items[i]].arg = (long long)end_target;
        }

        instr_emit(&out->code, OP_PUSH_I64, 0, 0);
        return 1;
    }
    case EXPR_BLOCK:
        return codegen_block(cg, fn, out, expr->as.block_expr.block, loop_ctx);
    }

    FATAL("internal code generation error at %d:%d", expr->line, expr->col);
    return 0;
}

static BytecodeProgram compile_program(Program *ast) {
    Codegen cg;
    memset(&cg, 0, sizeof(cg));
    cg.ast = ast;
    bytecode_program_init(&cg.program);

    for (size_t i = 0; i < ast->function_count; ++i) {
        Function *fn = ast->functions[i];
        BytecodeFunction *out = bytecode_program_push(&cg.program, fn->name);
        out->local_count = fn->local_count;
        out->param_count = (int)fn->param_count;
        out->return_type = fn->return_type;
        codegen_block(&cg, fn, out, fn->body, NULL);
        instr_emit(&out->code, OP_RET, 0, 0);
    }
    return cg.program;
}

/* Virtual machine state. */

typedef struct {
    int fn_index;
    size_t ip;
    int prev_sp;
} Frame;

typedef struct {
    BytecodeProgram *program;
    long long *stack;
    size_t stack_len;
    size_t stack_cap;
    Frame *frames;
    size_t frame_len;
    size_t frame_cap;
} VM;

static void vm_stack_ensure(VM *vm, size_t need) {
    if (need <= vm->stack_cap) {
        return;
    }
    size_t cap = vm->stack_cap == 0 ? 64 : vm->stack_cap;
    while (cap < need) {
        cap *= 2;
    }
    vm->stack = (long long *)xrealloc(vm->stack, cap * sizeof(long long));
    vm->stack_cap = cap;
}

static void vm_frame_ensure(VM *vm, size_t need) {
    if (need <= vm->frame_cap) {
        return;
    }
    size_t cap = vm->frame_cap == 0 ? 16 : vm->frame_cap;
    while (cap < need) {
        cap *= 2;
    }
    vm->frames = (Frame *)xrealloc(vm->frames, cap * sizeof(Frame));
    vm->frame_cap = cap;
}

static void vm_push(VM *vm, long long value) {
    vm_stack_ensure(vm, vm->stack_len + 1);
    vm->stack[vm->stack_len++] = value;
}

static long long vm_pop(VM *vm) {
    if (vm->stack_len == 0) {
        FATAL("vm stack underflow");
    }
    return vm->stack[--vm->stack_len];
}

static long long vm_run(BytecodeProgram *program) {
    if (program->count == 0) {
        FATAL("program contains no functions");
    }

    int main_index = -1;
    for (size_t i = 0; i < program->count; ++i) {
        if (strcmp(program->functions[i].name, "main") == 0) {
            main_index = (int)i;
            break;
        }
    }
    if (main_index < 0) {
        FATAL("missing main function");
    }

    VM vm;
    memset(&vm, 0, sizeof(vm));
    vm.program = program;

    /* Initialize the top-level frame for main. */
    vm_frame_ensure(&vm, 1);
    vm.frames[0] = (Frame){ main_index, 0, 0 };
    vm.frame_len = 1;
    vm_stack_ensure(&vm, (size_t)program->functions[main_index].local_count);
    vm.stack_len = (size_t)program->functions[main_index].local_count;
    for (size_t i = 0; i < vm.stack_len; ++i) {
        vm.stack[i] = 0;
    }

    for (;;) {
        if (vm.frame_len == 0) {
            break;
        }

        Frame *frame = &vm.frames[vm.frame_len - 1];
        BytecodeFunction *fn = &program->functions[frame->fn_index];
        if (frame->ip >= fn->code.len) {
            FATAL("instruction pointer out of range in function '%s'", fn->name);
        }
        Instr instr = fn->code.items[frame->ip++];
        switch (instr.op) {
        case OP_PUSH_I64:
            vm_push(&vm, instr.arg);
            break;
        case OP_LOAD:
            vm_push(&vm, vm.stack[(size_t)frame->prev_sp + (size_t)instr.arg]);
            break;
        case OP_STORE: {
            long long value = vm_pop(&vm);
            vm.stack[(size_t)frame->prev_sp + (size_t)instr.arg] = value;
            break;
        }
        case OP_POP:
            (void)vm_pop(&vm);
            break;
        case OP_ADD: {
            long long b = vm_pop(&vm);
            long long a = vm_pop(&vm);
            vm_push(&vm, a + b);
            break;
        }
        case OP_SUB: {
            long long b = vm_pop(&vm);
            long long a = vm_pop(&vm);
            vm_push(&vm, a - b);
            break;
        }
        case OP_MUL: {
            long long b = vm_pop(&vm);
            long long a = vm_pop(&vm);
            vm_push(&vm, a * b);
            break;
        }
        case OP_DIV: {
            long long b = vm_pop(&vm);
            long long a = vm_pop(&vm);
            if (b == 0) {
                FATAL("division by zero in runtime");
            }
            vm_push(&vm, a / b);
            break;
        }
        case OP_MOD: {
            long long b = vm_pop(&vm);
            long long a = vm_pop(&vm);
            if (b == 0) {
                FATAL("modulo by zero in runtime");
            }
            vm_push(&vm, a % b);
            break;
        }
        case OP_NEG:
            vm_push(&vm, -vm_pop(&vm));
            break;
        case OP_EQ: {
            long long b = vm_pop(&vm);
            long long a = vm_pop(&vm);
            vm_push(&vm, a == b);
            break;
        }
        case OP_NE: {
            long long b = vm_pop(&vm);
            long long a = vm_pop(&vm);
            vm_push(&vm, a != b);
            break;
        }
        case OP_LT: {
            long long b = vm_pop(&vm);
            long long a = vm_pop(&vm);
            vm_push(&vm, a < b);
            break;
        }
        case OP_LE: {
            long long b = vm_pop(&vm);
            long long a = vm_pop(&vm);
            vm_push(&vm, a <= b);
            break;
        }
        case OP_GT: {
            long long b = vm_pop(&vm);
            long long a = vm_pop(&vm);
            vm_push(&vm, a > b);
            break;
        }
        case OP_GE: {
            long long b = vm_pop(&vm);
            long long a = vm_pop(&vm);
            vm_push(&vm, a >= b);
            break;
        }
        case OP_NOT:
            vm_push(&vm, !vm_pop(&vm));
            break;
        case OP_JUMP:
            frame->ip = (size_t)instr.arg;
            break;
        case OP_JUMP_IF_FALSE: {
            long long cond = vm_pop(&vm);
            if (cond == 0) {
                frame->ip = (size_t)instr.arg;
            }
            break;
        }
        case OP_CALL: {
            int callee_index = (int)instr.arg;
            int argc = instr.aux;
            if (callee_index < 0 || (size_t)callee_index >= program->count) {
                FATAL("invalid call target");
            }
            BytecodeFunction *callee = &program->functions[callee_index];
            int prev_sp = (int)vm.stack_len - argc;
            if (prev_sp < 0) {
                FATAL("vm call with too few arguments");
            }

            /* Preserve the caller frame and create a new activation record. */
            vm_frame_ensure(&vm, vm.frame_len + 1);
            vm.frames[vm.frame_len++] = (Frame){ callee_index, 0, prev_sp };

            size_t needed = (size_t)prev_sp + (size_t)callee->local_count;
            vm_stack_ensure(&vm, needed);
            vm.stack_len = needed;
            for (size_t i = (size_t)prev_sp + (size_t)argc; i < needed; ++i) {
                vm.stack[i] = 0;
            }
            break;
        }
        case OP_RET: {
            long long result = vm_pop(&vm);
            Frame finished = vm.frames[vm.frame_len - 1];
            vm.frame_len--;
            vm.stack_len = (size_t)finished.prev_sp;
            vm_push(&vm, result);
            if (vm.frame_len == 0) {
                return vm_pop(&vm);
            }
            break;
        }
        }
    }

    FATAL("vm terminated unexpectedly");
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Public entry                                                               */
/* ------------------------------------------------------------------------- */

static char *read_file_text(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        FATAL("failed to open '%s'", path);
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        FATAL("failed to seek '%s'", path);
    }
    long size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        FATAL("failed to tell size of '%s'", path);
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        FATAL("failed to rewind '%s'", path);
    }
    char *text = (char *)xmalloc((size_t)size + 1);
    size_t read = fread(text, 1, (size_t)size, fp);
    fclose(fp);
    text[read] = '\0';
    return text;
}

int mini_rustc_run_file(const char *path, long long *out_result) {
    char *source = read_file_text(path);
    Program *ast = parse_program(source);
    sema_check_program(ast);
    BytecodeProgram program = compile_program(ast);
    long long result = vm_run(&program);
    if (out_result) {
        *out_result = result;
    }
    return 0;
}
