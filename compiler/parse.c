#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include "lex.h"
#include "parse.h"
#include "var.h"

void print_node (node_t* ast) {
    switch (ast->ntype) {
    case NODE_INTEG:
        printf("%li", ast->ival);
        break;
    case NODE_IBNOP:
        printf("(+ ");
        print_node(ast->left);
        printf(" ");
        print_node(ast->right);
        printf(")");
        break;
    }
}

static volatile int __trace__ = 1;
static volatile int __tok_trace__ = 0;
static volatile int __parse_globe__ = 0;

node_t* parse_expr (node_t* ast, tokstring_t* toks);
node_t* parse_cpexpr (node_t* ast, tokstring_t* toks);
node_t* parse_paramlist (node_t* ast, tokstring_t* toks, clauses_t* clauses);
node_t* parse_unary (node_t* ast, tokstring_t* toks);
node_t* parse_binary (node_t* ast, tokstring_t* toks);
clauses_t* parse_make_clauses (clauses_t* clauses, int cap);
node_t* parse_call (node_t* ast, tokstring_t* toks, clauses_t* clauses);
void parse_cond (node_t* cond, tokstring_t* toks, clauses_t* clauses);
char* parse_name (tokstring_t* toks);
void parse_bindings (node_t* ast, tokstring_t* toks, clauses_t* clauses);
node_t* parse_multivar (node_t* ast, tokstring_t* toks, tok_t tok);
void parse_setvalue(node_t* expr, token_t tok, etype_t type);
int64_t safe_atoi(char* val);
int parse_got (tokstring_t* toks, tok_t tok);
void parse_want (tokstring_t* toks, tok_t tok);
int tok_get (tokstring_t* toks);
token_t token_get (tokstring_t* toks);
void tok_consume (tokstring_t* toks);
void parse_error (node_t* node, char* msg, ...);
void tok_error (tokstring_t* toks, char* msg, ...);
char* type_name (etype_t type);
void free_node (node_t* node, int parr);
void parse_quote (node_t* ast, tokstring_t* toks);
clauses_t* parse_clause_append (clauses_t* clauses, node_t binding);
void parse_quasiquote (node_t* ast, tokstring_t* toks);
int sym_is_string(char** val_);
int sym_is_bool(char* val);
int sym_is_int(char* val);
void parse_multivar_right (node_t** ast, tokstring_t* toks);
node_t* parse_pattern(node_t* arg, node_t* ast, tokstring_t* toks);
node_t* parse_pattern_bind(node_t* arg, node_t* ast, node_t* expr, tokstring_t* toks);
node_t* parse_andor (node_t* ast);
node_t* parse_match (node_t* ast);
void parse_printer(node_t* ast);
char* optostring(otype_t op);

static volatile int __pgensym = 1;

char* pgensym (char* buf, char* pre) {
    sprintf(buf, "%s%d", pre, ++__pgensym);
    return buf;
}

node_t* newnode (node_t* node) {
    node->l = 0;
    node->c = 0;
    node->ntype = NODE_UNDEF;
    node->left = NULL;
    node->right = NULL;
    node->cond = NULL;
    node->clauses = NULL;
    node->sval = NULL;
    node->bval = 0;
    node->ival = 0;
    node->etype = TYPE_U;
    node->otype = OP_UND;
    node->supress = 0;

    return node;
}

/**
 * For now, all programs are simply expressions.
 * Eventually, programs will consist of an ordered
 * list of expressions to be evaluated sequentially.
 * Each might introduce global bindings as a 
 * function, closure (function + persistent 
 * environment), or an expression to be evaluated.
 * Essentially these are all expressions. But the
 * first two (function and closure) have the side
 * effect of mutating the global environment.
 * 
 * Compiled programs can either represent a library
 * (dynamic), package (static), or a program 
 * (executable). Programs have an entry point and
 * cannot be included as a library or package.
 * Packages create an object file to be linked with
 * programs or libraries (but cannot be in both!).
 * Additionally, package globals MUST be immutable.
 * Libraries modify the core runtime by adding fully
 * persistent globals across all programs which 
 * include the library. Libraries cannot include 
 * packages and can only reference other libraries.
 */
node_t* parse (tokstring_t* toks) {
    node_t* ast = malloc(sizeof(node_t));
    node_t* expr;
    newnode(ast);
    toks->off = 0;


    ast->clauses = malloc(sizeof(clauses_t));
    ast->clauses->len = 0;
    ast->clauses->cap = 0;
    ast->clauses->clauses = NULL;
    ast->ntype = NODE_BEGIN;

    for (int i = 0; i < toks->len; i++) {
        if (__tok_trace__) {
            printf("[%d:%d] %s\n", toks->toks[i].l, toks->toks[i].c, tok_name(toks->toks[i].tok));
        }
    }

    parse_make_clauses(ast->clauses, 4);

    /* now parses until endofl */
    while (!parse_got(toks, TOK_ENDOFL) && (toks->off < (toks->len - 1))) {
        if (2 * ast->clauses->len >= ast->clauses->cap) {
            parse_make_clauses(ast->clauses, 0);
        }
        
        __parse_globe__ = 1;
        node_t* res = newnode(malloc(sizeof(node_t)));
        res = parse_expr(res, toks);
        __parse_globe__ = 0;
        if (tok_get(toks) == TOK_SUPRSS) {
            tok_consume(toks);
            res->supress = 1;
        }
        ast->clauses->clauses[ast->clauses->len++] = *res;
    }

    /* ast = parse_expr(ast, toks); */
    if (__trace__) { printf("\n\n=============== TREE 0 ===============\n\n"); parse_printer(ast); }
    //sleep(1);
    /* we need to reduce the AST match s-expressions now */
    ast = parse_match(ast);

    if (__trace__) { printf("\n\n=============== TREE 1 ===============\n\n"); parse_printer(ast); }

    /* finally, we need to reduce AST and/or s-expressions */
    ast = parse_andor(ast);

    if (__trace__) { printf("\n\n=============== TREE 2 ===============\n\n"); parse_printer(ast); }

    /* we should free tokstring now */
    free(toks->toks);

    return ast;
}

node_t* parse_expr (node_t* ast, tokstring_t* toks) {
    int __parse_globe = __parse_globe__;
    __parse_globe__ = 0;
    switch (tok_get(toks)) {
    case TOK_MTCHDF:
        ast->l = token_get(toks).l;
        ast->c = token_get(toks).c;
        ast->ntype = NODE_DEFLT;
        tok_consume(toks);
        break;
    case TOK_LPAREN:
        if (__trace__) {
            printf("parse_expr\n");
        }
        tok_consume(toks);
        ast = parse_cpexpr(ast, toks);
        if (!parse_got(toks, TOK_RPAREN)) {
            parse_error(ast, "invalid parenthetical expression!");
        }
        break;
    case TOK_NUMLIT:
        if (__trace__) {
            printf("parse_number\n");
        }
        /* node_t* pexpr = newnode(malloc(sizeof(node_t))); */
        ast->ntype = NODE_INTEG;
        parse_setvalue(ast, token_get(toks), TYPE_I);
        tok_consume(toks);
        break;
    case TOK_STRLIT:
        if (__trace__) {
            printf("parse_string\n");
        }
        ast->ntype = NODE_STRNG;
        parse_setvalue(ast, token_get(toks), TYPE_S);
        tok_consume(toks);
        break;
    case TOK_BVTRUE:
        if (__trace__) {
            printf("parse_bool\n");
        }
        ast->ntype = NODE_BOOLV;
        parse_setvalue(ast, token_get(toks), TYPE_B);
        tok_consume(toks);
        break;
    case TOK_BVFALS:
        if (__trace__) {
            printf("parse_bool\n");
        }
        ast->ntype = NODE_BOOLV;
        parse_setvalue(ast, token_get(toks), TYPE_B);
        tok_consume(toks);
        break;
    case TOK_IDENTF:
        if (__trace__) {
            printf("parse_identifier %s\n", token_get(toks).val);
        }
        ast->ntype = NODE_VARBL;
        parse_setvalue(ast, token_get(toks), TYPE_S);
        tok_consume(toks);
        break;
    case TOK_UNQUOT:
        if (__trace__) {
            printf("parse_unquote_operation\n");
        }
        ast->ntype = NODE_UQOPR;
        ast->l = token_get(toks).l;
        ast->c = token_get(toks).c;
        tok_consume(toks);
        ast->left = newnode(malloc(sizeof(node_t)));
        ast->left = parse_expr(ast->left, toks);
        break;
    case TOK_LQUOTE:
        ast->l = token_get(toks).l;
        ast->c = token_get(toks).c;
        ast->ntype = NODE_LQUOT;
        tok_consume(toks);
        if (tok_get(toks) == TOK_LPAREN) {
            ast->ntype = NODE_CQUOT;
            tok_consume(toks);
            parse_quote(ast, toks);
            if (__trace__) { printf("quote-list length: %d\n", ast->clauses->len); }
            break;
        }
        else {
            if (__trace__) {
                printf("parse_quote %s\n", token_get(toks).val);
            }
            ast->sval = token_get(toks).val;
            if (sym_is_string(&ast->sval)) {
                /* sval now stores properly formatted string */
                ast->ntype = NODE_LQSTR;
                if (__trace__) { printf("parse_quote string %s\n",  ast->sval); }
            }
            else if (sym_is_bool( ast->sval)) {
                ast->ntype = NODE_LQFLG;
                ast->bval = ((!strcmp( ast->sval, "#t")) ? 1 : 0);
                free( ast->sval);
            }
            else if (sym_is_int( ast->sval)) {
                ast->ntype = NODE_LQINT;
                ast->ival = safe_atoi( ast->sval);
                free( ast->sval);
            }
            tok_consume(toks);
        }
        break;
    case TOK_QQUOTE:
        ast->l = token_get(toks).l;
        ast->c = token_get(toks).c;
        ast->ntype = NODE_LQUOT;
        tok_consume(toks);
        if (tok_get(toks) == TOK_LPAREN) {
            ast->ntype = NODE_CQUOT;
            tok_consume(toks);
            parse_quasiquote(ast, toks);
            if (__trace__) { printf("quote-list length: %d\n", ast->clauses->len); }
            break;
        }
        else if (tok_get(toks) == TOK_UNQUOT) {
            if (__trace__) {
                printf("parse_quotify\n");
            }
            ast->ntype = NODE_QTOPR;
            ast->left = newnode(malloc(sizeof(node_t)));
            ast->l = token_get(toks).l;
            ast->c = token_get(toks).c;
            tok_consume(toks);
            ast->left = parse_expr(ast->left, toks);
            break;
        }
        else {
            if (__trace__) {
                printf("parse_quote %s\n", token_get(toks).val);
            }
            ast->sval = token_get(toks).val;
            if (sym_is_string(&ast->sval)) {
                /* sval now stores properly formatted string */
                ast->ntype = NODE_LQSTR;
                if (__trace__) { printf("parse_quote string %s\n",  ast->sval); }
            }
            else if (sym_is_bool( ast->sval)) {
                ast->ntype = NODE_LQFLG;
                ast->bval = ((!strcmp( ast->sval, "#t")) ? 1 : 0);
                free( ast->sval);
            }
            else if (sym_is_int( ast->sval)) {
                ast->ntype = NODE_LQINT;
                ast->ival = safe_atoi( ast->sval);
                free( ast->sval);
            }
            tok_consume(toks);
        }
        break;
    case TOK_EMPTYL:
        if (__trace__) {
            printf("parse_emptylist\n");
        }
        ast->ntype = NODE_ELIST;
        ast->l = token_get(toks).l;
        ast->c = token_get(toks).c;
        tok_consume(toks);
        break;
    default:
        parse_error(ast, "unexpected %s in expression", tok_name(tok_get(toks)));
    }

    if (!__parse_globe && tok_get(toks) == TOK_SUPRSS) {
        /* treat it as a multi-statement */
        tok_consume(toks);
        node_t* multi = newnode(malloc(sizeof(node_t)));
        multi->l = ast->l;
        multi->c = ast->c;
        multi->ntype = NODE_MULTI;
        multi->left = ast;
        multi->right = parse_expr(newnode(malloc(sizeof(node_t))), toks);
        ast = multi;
    }
    __parse_globe__ = __parse_globe;
    return ast;
}

node_t* parse_cpexpr (node_t* ast, tokstring_t* toks) {
    /* trace */
    if (__trace__) {
        printf("parse_cpexpr\n");
    }
    tok_t tok;
    node_t* expr;
    node_t* cond;
    clauses_t* clauses;
    switch (tok_get(toks)) {
    case TOK_MUTSET:
        ast->l = token_get(toks).l;
        ast->c = token_get(toks).c;
        ast->ntype = NODE_MUTST;
        tok_consume(toks);
        ast->sval = parse_name(toks);
        ast->left = newnode(malloc(sizeof(node_t)));
        ast->left = parse_expr(ast->left, toks);
        break;
    case TOK_MATCHE:
        ast->l = token_get(toks).l;
        ast->c = token_get(toks).c;
        ast->ntype = NODE_MATCH;
        tok_consume(toks);
        ast->left = newnode(malloc(sizeof(node_t)));
        ast->left = parse_expr(ast->left, toks);
        ast->clauses = NULL;
        while (tok_get(toks) != TOK_RPAREN) {
            node_t __node3; 
            newnode(&__node3);
            __node3.l = token_get(toks).l;
            __node3.c = token_get(toks).c;
            __node3.ntype = NODE_MTCHC;
            parse_want(toks, TOK_LPAREN);
            node_t* __node1 = parse_expr(newnode(malloc(sizeof(node_t))), toks);
            node_t* __node2 = parse_expr(newnode(malloc(sizeof(node_t))), toks);
            __node3.left = __node1;
            __node3.right = __node2;
            ast->clauses = parse_clause_append(ast->clauses, __node3);
            parse_want(toks, TOK_RPAREN);
        }
        break;
    case TOK_INCOPR:
    case TOK_DECOPR:
    case TOK_ABSOPR:
    case TOK_SUBOPR: 
    case TOK_ADDOPR: 
    case TOK_MULOPR:
    case TOK_DIVOPR:
    case TOK_ZEROIF:
    case TOK_CARCAR:
    case TOK_CDRCDR:
    case TOK_BOXBOX:
    case TOK_UNBOXE:
    case TOK_FNPROC:
    case TOK_CHKSTR:
    case TOK_CHKLST:
    case TOK_CHKELS:
    case TOK_CHKINT:
    case TOK_CHKFLG:
    case TOK_CHKBOO:
    case TOK_CHKSYM:
    case TOK_CHKPRC:
    case TOK_CHKBOX:
    case TOK_CHKBYT:
    case TOK_ANDAND:
    case TOK_OROROR:
    case TOK_LENGTH:
    case TOK_NOTNOT:
    case TOK_PRTOPR:
    case TOK_CHKCNS:
        ast = parse_unary(ast, toks);
        break;
    case TOK_GTCOMP:
    case TOK_GECOMP:
    case TOK_LECOMP:
    case TOK_LTCOMP:
    case TOK_EQCOMP:
    case TOK_NECOMP:
    case TOK_CONSLT:
    case TOK_STREQL:
    case TOK_SYMEQL:
    case TOK_MODMOD:
    case TOK_BITAND:
    case TOK_BITLOR:
    case TOK_BITXOR:
    case TOK_BITSHL:
    case TOK_BITSHR:
    case TOK_SYMAPP:
        ast = parse_binary(ast, toks);
        break;
    case TOK_IFWORD:
        if (__trace__) {
            printf("parse_if\n");
        }
        ast->l = token_get(toks).l;
        ast->c = token_get(toks).c;
        ast->ntype = NODE_IFEXP;
        tok = tok_get(toks);
        tok_consume(toks);
        cond = newnode(malloc(sizeof(node_t)));
        cond = parse_expr(cond, toks);
        ast->cond = cond;
        /* next get yes, no */
        ast->left = newnode(malloc(sizeof(node_t)));
        ast->right = newnode(malloc(sizeof(node_t)));
        ast->left = parse_expr(ast->left, toks);
        ast->right = parse_expr(ast->right, toks);
        break;
    case TOK_CONDIT:
        /* begin next thingie */
        ast->ntype = NODE_CONDE;
        tok = tok_get(toks);
        ast->l = token_get(toks).l;
        ast->c = token_get(toks).c;
        tok_consume(toks);
        clauses = malloc(sizeof(clauses_t));
        clauses->cap = 0;
        clauses->len = 0;
        clauses->clauses = NULL;
        parse_want(toks, TOK_LPAREN);
        parse_cond(ast, toks, parse_make_clauses(clauses, 4));
        ast->clauses = clauses;
        parse_want(toks, TOK_RPAREN);
        break;
    case TOK_LETLET:
        /* woooooooo */
        ast->ntype = NODE_LETEX;
        tok = tok_get(toks);
        ast->l = token_get(toks).l;
        ast->c = token_get(toks).c;
        tok_consume(toks);
        clauses = malloc(sizeof(clauses_t));
        clauses->cap = 0;
        clauses->len = 0;
        clauses->clauses = NULL;
        parse_want(toks, TOK_LPAREN);
        parse_bindings(ast, toks, parse_make_clauses(clauses, 4));
        ast->clauses = clauses;
        parse_want(toks, TOK_RPAREN);
        /* now we have an expression */
        ast->left = newnode(malloc(sizeof(node_t)));
        ast->left = parse_expr(ast->left, toks);
        break;
    case TOK_DEFINE:
        ast->ntype = NODE_DEFFN;
        ast->l = token_get(toks).l;
        ast->c = token_get(toks).c;
        tok_consume(toks);
        if (tok_get(toks) == TOK_IDENTF) {
            /* global */
            ast->ntype = NODE_GLOBL;
            ast->sval = parse_name(toks);
            ast->left = parse_expr(newnode(malloc(sizeof(node_t))), toks);
            break;
        }
        clauses = malloc(sizeof(clauses_t));
        clauses->cap = 0;
        clauses->len = 0;
        clauses->clauses = NULL;
        parse_want(toks, TOK_LPAREN);
        ast->sval = parse_name(toks);
        parse_paramlist(ast, toks, parse_make_clauses(clauses, 4));
        parse_want(toks, TOK_RPAREN);
        ast->clauses = clauses;

        /* right-recursive tree of expressions */
        //node_t* pnode = ast;
        ast->left = parse_expr(newnode(malloc(sizeof(node_t))), toks);
        //if (tok_get(toks) == TOK_SUPRSS) {
        //    ast->right = ast->left;
        //    ast->left = NULL;
        //}
        //while (tok_get(toks) == TOK_SUPRSS) {
        //    /* imperative-style */
        //    tok_consume(toks);
        //    node_t* nnode = newnode(malloc(sizeof(node_t)));
        //    nnode = parse_expr(nnode, toks);
        //    pnode->supress = 1;
        //    if (tok_get(toks) == TOK_SUPRSS) {
        //        pnode->right = nnode;
        //        pnode = nnode;
        //    }
        //    else {
        //        ast->left = nnode;
        //    }
        //}        

        break;
    case TOK_LISTBD:
        /* gets converted to ANF cons */
        ast->ntype = NODE_ELIST;
        ast->l = token_get(toks).l;
        ast->c = token_get(toks).c;
        tok_consume(toks);
        if (tok_get(toks) != TOK_RPAREN) {
            ast->ntype = NODE_IBNOP;
            ast->left = newnode(malloc(sizeof(node_t)));
            ast->right = newnode(malloc(sizeof(node_t)));
            ast->left = parse_expr(ast->left, toks);
            ast->right->l = ast->l;
            ast->right->c = ast->c;
            ast->right->ntype = NODE_ELIST;
            ast->otype = OP_CNS;
            if (tok_get(toks) != TOK_RPAREN) {
                free(ast->right);
                ast->right = NULL;
                parse_multivar_right(&ast, toks);
            }
        }
        break;
    case TOK_QUMARK:
        {
            ast->ntype = NODE_QMARK;
            ast->l = token_get(toks).l;
            ast->c = token_get(toks).c;
            node_t* expr = newnode(malloc(sizeof(node_t)));
            tok_consume(toks);
            expr->ntype = NODE_IUNOP;
            expr->left = NULL;
            expr->l = token_get(toks).l;
            expr->c = token_get(toks).c;
            switch (tok_get(toks)) {
            case TOK_CHKBOO:
                expr->otype = CHKBOO;
                break;
            case TOK_CHKBOX:
                expr->otype = CHKBOX;
                break;
            case TOK_CHKBYT:
                expr->otype = CHKBYT;
                break;
            case TOK_CHKCNS:
                expr->otype = CHKCNS;
                break;
            case TOK_CHKELS:
                expr->otype = CHKELS;
                break;
            case TOK_CHKFLG:
                expr->otype = CHKFLG;
                break;
            case TOK_CHKINT:
                expr->otype = CHKINT;
                break;
            case TOK_CHKLST:
                expr->otype = CHKLST;
                break;
            case TOK_CHKPRC:
                expr->otype = CHKPRC;
                break;
            case TOK_CHKSTR:
                expr->otype = CHKSTR;
                break;
            case TOK_CHKSYM:
                expr->otype = CHKSYM;
                break;
            case TOK_IDENTF:
                expr->ntype = NODE_CALLF;
                expr->sval = token_get(toks).val;
                expr->clauses = NULL;
                break;
            default:
                parse_error(ast, "invalid match question case!");
            }

            tok_consume(toks);

            ast->left = expr;
            
            if (tok_get(toks) == TOK_IDENTF) {
                /* store identifier in ast->left */
                node_t* varbl = newnode(malloc(sizeof(node_t)));
                varbl->l = token_get(toks).l;
                varbl->c = token_get(toks).c;
                varbl->sval = token_get(toks).val;
                varbl->ntype = NODE_VARBL;
                ast->right = varbl;
                tok_consume(toks);
            }
        }
        break;
    case TOK_IDENTF:
        if (__trace__) { printf("parse_calldef %s\n", token_get(toks).val); }
        ast->ntype = NODE_CALLF;
        ast->l = token_get(toks).l;
        ast->c = token_get(toks).c;
        clauses = malloc(sizeof(clauses_t));
        clauses->cap = 0;
        clauses->len = 0;
        clauses->clauses = NULL;
        ast->sval = parse_name(toks);
        parse_call(ast, toks, parse_make_clauses(clauses, 4));
        ast->clauses = clauses;
        ast->cond = NULL;
        break;
    case TOK_LPAREN:
        if (__trace__) { printf("parse_callfun\n"); }
        ast->ntype = NODE_CALLF;
        ast->l = token_get(toks).l;
        ast->c = token_get(toks).c;
        clauses = malloc(sizeof(clauses_t));
        clauses->cap = 0;
        clauses->len = 0;
        clauses->clauses = NULL;
        ast->cond = parse_expr(newnode(malloc(sizeof(node_t))), toks);
        parse_call(ast, toks, parse_make_clauses(clauses, 4));
        ast->clauses = clauses;
        break;
    case TOK_FLOPEN:
        ast->ntype = NODE_FLOPN;
        ast->l = token_get(toks).l;
        ast->c = token_get(toks).c;
        tok_consume(toks);
        ast->left = parse_expr(newnode(malloc(sizeof(node_t))), toks);
        break;
    case TOK_FLWRIT:
        ast->ntype = NODE_FLWRT;
        ast->l = token_get(toks).l;
        ast->c = token_get(toks).c;
        tok_consume(toks);
        ast->left = parse_expr(newnode(malloc(sizeof(node_t))), toks);
        break;
    case TOK_FLCLOS:
        ast->ntype = NODE_FLCLS;
        ast->l = token_get(toks).l;
        ast->c = token_get(toks).c;
        tok_consume(toks);
        break;
    case TOK_LAMBDA:
        if (__trace__) {
            printf("parse_lambda\n");
        }
        ast->ntype = NODE_CLOSE;
        ast->l = token_get(toks).l;
        ast->c = token_get(toks).c;
        tok_consume(toks);
        clauses = malloc(sizeof(clauses_t));
        clauses->cap = 0;
        clauses->len = 0;
        clauses->clauses = NULL;
        parse_want(toks, TOK_LPAREN);
        parse_paramlist(ast, toks, parse_make_clauses(clauses, 4));
        parse_want(toks, TOK_RPAREN);
        ast->clauses = clauses;
        ast->left = parse_expr(newnode(malloc(sizeof(node_t))), toks);
        break;
    default:
        parse_error(ast, "unexpected %s", tok_name(tok));
    }
    return ast;
}

node_t* parse_paramlist (node_t* ast, tokstring_t* toks, clauses_t* clauses) {
    if (__trace__) { printf("parse_paramlist\n"); }
    if (tok_get(toks) == TOK_RPAREN) {
        return ast;
    }
    node_t param;
    newnode(&param);
    param.l = token_get(toks).l;
    param.c = token_get(toks).c;
    param.ntype = NODE_VARBL;
    param.sval = parse_name(toks);

    if (2 * clauses->len >= clauses->cap) {
        parse_make_clauses(clauses, 0);
    }
    clauses->clauses[clauses->len++] = param;

    if (tok_get(toks) != TOK_RPAREN) {
        return parse_paramlist(ast, toks, clauses);
    }
}

node_t* parse_call (node_t* ast, tokstring_t* toks, clauses_t* clauses) {
    if (tok_get(toks) == TOK_RPAREN) {
        return ast;
    }
    node_t *arg = newnode(malloc(sizeof(node_t)));

    if (2 * clauses->len >= clauses->cap) {
        parse_make_clauses(clauses, 0);
    }
    
    clauses->clauses[clauses->len++] = *parse_expr(arg, toks);
    free(arg);
    if (tok_get(toks) != TOK_RPAREN) {
        return parse_call(ast, toks, clauses);
    }
}

node_t* parse_unary (node_t* ast, tokstring_t* toks) {
    tok_t tok = tok_get(toks);
    node_t* expr = newnode(malloc(sizeof(node_t)));
    tok_consume(toks);
    ast->l = token_get(toks).l;
    ast->c = token_get(toks).c;
    ast->ntype = NODE_IUNOP;
    ast->left = parse_expr(expr, toks);
    switch (tok) {
    case TOK_PRTOPR:
        if (__trace__) {
            printf("parse_print\n");
        }
        ast->otype = OP_PRT;
        break;
    case TOK_INCOPR:
        if (__trace__) {
            printf("parse_inc\n");
        }
        ast->otype = OP_INC;
        break;
    case TOK_NOTNOT:
        if (__trace__) {
            printf("parse_not\n");
        }
        ast->otype = OP_NOT;
        break;
    case TOK_CHKCNS:
        if (__trace__) {
            printf("parse_check_cons\n");
        }
        ast->otype = CHKCNS;
        break;
    case TOK_DECOPR:
        if (__trace__) {
            printf("parse_dec\n");
        }
        ast->otype = OP_DEC;
        break;
    case TOK_ABSOPR:
        if (__trace__) {
            printf("parse_abs\n");
        }
        ast->otype = OP_ABS;
        break;
    case TOK_SUBOPR: /* univariate=negop, multivariate=subop */
        if (__trace__) {
            printf("parse_neg-\n");
        }
        ast->otype = OP_NEG;
        if (tok_get(toks) != TOK_RPAREN) {
            /* assume multivariate sub */
            ast->ntype = NODE_IBNOP;
            ast->otype = OP_SUB;
            ast = parse_multivar(ast, toks, tok);
        }
        break;
    case TOK_MULOPR: 
        if (__trace__) {
            printf("parse_mul*\n");
        }
        ast->otype = OP_ABS;
        if (tok_get(toks) != TOK_RPAREN) {
            ast->ntype = NODE_IBNOP;
            ast->otype = OP_MUL;
            ast = parse_multivar(ast, toks, tok);
        }
        break;
    case TOK_DIVOPR: 
        if (__trace__) {
            printf("parse_div*\n");
        }
        ast->otype = OP_ABS;
        if (tok_get(toks) != TOK_RPAREN) {
            ast->ntype = NODE_IBNOP;
            ast->otype = OP_DIV;
            ast = parse_multivar(ast, toks, tok);
        }
        break;
    case TOK_ADDOPR: 
        if (__trace__) {
            printf("parse_abs+\n");
        }
        ast->otype = OP_ABS;
        if (tok_get(toks) != TOK_RPAREN) {
            /* assume multivariate add */
            ast->ntype = NODE_IBNOP;
            ast->otype = OP_ADD;
            ast = parse_multivar(ast, toks, tok);
        }
        break;
    case TOK_ANDAND: 
        if (__trace__) {
            printf("parse_and+\n");
        }
        ast->otype = OP_AND;
        ast->clauses = NULL;
        ast->clauses = parse_clause_append(ast->clauses, *(ast->left));
        free(ast->left);
        while (tok_get(toks) != TOK_RPAREN) {
            node_t __node;
            ast->clauses = parse_clause_append(ast->clauses, *parse_expr(newnode(&__node), toks));
        }
        break;
    case TOK_OROROR: 
        if (__trace__) {
            printf("parse_or+\n");
        }
        ast->otype = OP_LOR;
        ast->clauses = NULL;
        ast->clauses = parse_clause_append(ast->clauses, *(ast->left));
        free(ast->left);
        while (tok_get(toks) != TOK_RPAREN) {
            node_t __node;
            ast->clauses = parse_clause_append(ast->clauses, *parse_expr(newnode(&__node), toks));
        }
        break;
    case TOK_CHKSTR:
        if (__trace__) {
            printf("parse_string?\n");
        }
        ast->otype = CHKSTR;
        break; 
    case TOK_CHKLST:
        if (__trace__) {
            printf("parse_list?\n");
        }
        ast->otype = CHKLST;
        break; 
    case TOK_CHKELS:
        if (__trace__) {
            printf("parse_emptylist?\n");
        }
        ast->otype = CHKELS;
        break; 
    case TOK_CHKINT:
        if (__trace__) {
            printf("parse_integer?\n");
        }
        ast->otype = CHKINT;
        break; 
    case TOK_CHKFLG:
        if (__trace__) {
            printf("parse_flag?\n");
        }
        ast->otype = CHKFLG;
        break; 
    case TOK_CHKBOO:
        if (__trace__) {
            printf("parse_boolean?\n");
        }
        ast->otype = CHKBOO;
        break; 
    case TOK_CHKSYM:
        if (__trace__) {
            printf("parse_symbol?\n");
        }
        ast->otype = CHKSYM;
        break; 
    case TOK_CHKPRC:
        if (__trace__) {
            printf("parse_proc?\n");
        }
        ast->otype = CHKPRC;
        break; 
    case TOK_CHKBOX:
        if (__trace__) {
            printf("parse_box?\n");
        }
        ast->otype = CHKBOX;
        break; 
    case TOK_CHKBYT:
        if (__trace__) {
            printf("parse_bytes?\n");
        }
        ast->otype = CHKBYT;
        break; 
    case TOK_LENGTH:
        if (__trace__) {
            printf("parse_length\n");
        }
        ast->otype = OP_LEN;
        break; 
    case TOK_ZEROIF:
        if (__trace__) {
            printf("parse_zero\n");
        }
        ast->otype = CMP_ZR;
        break;
    case TOK_CARCAR:
        if (__trace__) {
            printf("parse_car\n");
        }
        ast->otype = OP_CAR;
        break;
    case TOK_CDRCDR:
        if (__trace__) {
            printf("parse_cdr\n");
        }
        ast->otype = OP_CDR;
        break;
    case TOK_BOXBOX:
        if (__trace__) {
            printf("parse_box\n");
        }
        ast->otype = OP_BOX;
        break;
    case TOK_UNBOXE:
        if (__trace__) {
            printf("parse_unbox\n");
        }
        ast->otype = OP_UBX;
        break;
    default:
        parse_error(ast, "unexpected %s ; fatal error.", tok_name(tok));
    }
    return ast;
}

void free_node(node_t* node, int parr) {
    if (node != NULL) {
        if (node->left != NULL) {
            free_node(node->left, 1);
        }
        if (node->right != NULL) {
            free_node(node->right, 1);
        }
        if (node->cond != NULL) {
            free_node(node->cond, 1);
        }
        if (node->clauses != NULL) {
            for (int i = 0; i < node->clauses->len; i++) {
                free_node(&(node->clauses->clauses[i]), 0);
            }
        }
        if (parr) {
            free(node);
        }
    }
}

node_t* parse_binary (node_t* ast, tokstring_t* toks) {
    tok_t tok = tok_get(toks);
    tok_consume(toks);
    ast->l = token_get(toks).l;
    ast->c = token_get(toks).c;
    ast->ntype = NODE_IBNOP;
    ast->left = parse_expr(newnode(malloc(sizeof(node_t))), toks);
    ast->right = parse_expr(newnode(malloc(sizeof(node_t))), toks);
    switch (tok) {
    case TOK_SYMAPP:
        ast->otype = OP_SAP;
        break;
    case TOK_GTCOMP:
        if (__trace__) {
            printf("parse_greaterthan\n");
        }
        ast->otype = CMP_GT;
        break;
    case TOK_GECOMP:
        if (__trace__) {
            printf("parse_greaterequal\n");
        }
        ast->otype = CMP_GE;
        break;
    case TOK_LECOMP:
        if (__trace__) {
            printf("parse_lessequal\n");
        }
        ast->otype = CMP_LE;
        break;
    case TOK_LTCOMP:
        if (__trace__) {
            printf("parse_lessthan\n");
        }
        ast->otype = CMP_LT;
        break;
    case TOK_EQCOMP:
        if (__trace__) {
            printf("parse_equals\n");
        }
        ast->otype = CMP_EQ;
        break;
    case TOK_NECOMP:
        if (__trace__) {
            printf("parse_notequals\n");
        }
        ast->otype = CMP_NE;
        break;
    case TOK_CONSLT:
        if (__trace__) {
            printf("parse_cons\n");
        }
        ast->otype = OP_CNS;
        break;
    case TOK_SYMEQL:
        if (__trace__) {
            printf("parse_symbol=?\n");
        }
        ast->otype = CMP_SE;
        break;
    case TOK_STREQL:
        if (__trace__) {
            printf("parse_string=?\n");
        }
        ast->otype = CMP_RE;
        break;
    case TOK_MODMOD:
        if (__trace__) {
            printf("parse_mod\n");
        }
        ast->otype = OP_MOD;
        break;
    case TOK_BITAND:
        if (__trace__) {
            printf("parse_bitand\n");
        }
        ast->otype = BITAND;
        break;
    case TOK_BITLOR:
        if (__trace__) {
            printf("parse_bitor\n");
        }
        ast->otype = BITLOR;
        break;
    case TOK_BITXOR:
        if (__trace__) {
            printf("parse_bitxor\n");
        }
        ast->otype = BITXOR;
        break;
    case TOK_BITSHL:
        if (__trace__) {
            printf("parse_bitshl\n");
        }
        ast->otype = BITSHL;
        break;
    case TOK_BITSHR:
        if (__trace__) {
            printf("parse_bitshr\n");
        }
        ast->otype = BITSHR;
        break;
    default:
        parse_error(ast, "unexpected %s ; fatal error.", tok_name(tok));
    }
    return ast;
}

clauses_t* parse_make_clauses (clauses_t* clauses, int cap) {
    node_t* new_clauses;
    if (clauses->len == 0) {
        clauses->cap = cap;
        clauses->clauses = malloc(sizeof(node_t) * cap);
    }
    else {
        cap = 2 * clauses->cap;
        new_clauses = malloc(sizeof(node_t) * cap);
        memcpy((uint8_t*) new_clauses, (uint8_t*) clauses->clauses, sizeof(node_t) * clauses->len);
        free(clauses->clauses);
        clauses->clauses = new_clauses;
        clauses->cap = cap;
    }
    return clauses;
}

// cond_expr = '(' cond '(' clauses else_expr ')' ')'
void parse_cond (node_t* cond, tokstring_t* toks, clauses_t* clauses) {
    if (__trace__) {
        printf("parse_cond\n");
    }
    parse_want(toks, TOK_LPAREN);
    if (tok_get(toks) == TOK_ELSEWD) {
        if (__trace__) {
            printf("parse_else\n");
        }
        tok_consume(toks);
        node_t* else_ = newnode(malloc(sizeof(node_t)));
        else_ = parse_expr(else_, toks);
        cond->cond = else_;
        parse_want(toks, TOK_RPAREN);
    } else {
        if (__trace__) {
            printf("parse_clause\n");
        }
        
        int clausel = token_get(toks).l;
        int clausec = token_get(toks).c;
        node_t* cl = newnode(malloc(sizeof(node_t)));
        node_t* cr = newnode(malloc(sizeof(node_t)));
        cl = parse_expr(cl, toks);
        cr = parse_expr(cr, toks);
        node_t clause;
        newnode(&clause);
        clause.left = cl;
        clause.right = cr;
        clause.etype = cr->etype;
        clause.ntype = NODE_CONDC;
        clause.l = clausel;
        clause.c = clausec;

        parse_want(toks, TOK_RPAREN);

        /* add clause */
        if (2 * clauses->len >= clauses->cap) {
            parse_make_clauses(clauses, 0);
        }
        clauses->clauses[clauses->len++] = clause;

        parse_cond(cond, toks, clauses);
    }
}

char* parse_name (tokstring_t* toks) {
    char* rv_;
    if (tok_get(toks) != TOK_IDENTF) {
        tok_error(toks, "expected an identifier");
    }
    rv_ = token_get(toks).val;
    tok_consume(toks);
    return rv_;
}

void parse_bindings (node_t* ast, tokstring_t* toks, clauses_t* clauses) {
    if (__trace__) {
        printf("parse_bindings\n");
    }
    int ln = token_get(toks).l;
    int cl = token_get(toks).c;
    parse_want(toks, TOK_LPAREN);
    /* commencing operations ;) */
    node_t binding;
    newnode(&binding);
    char* name = parse_name(toks);
    node_t* expr = newnode(malloc(sizeof(node_t)));
    expr = parse_expr(expr, toks);
    binding.sval = name;
    binding.left = expr;
    binding.ntype = NODE_BINDE;
    binding.l = ln;
    binding.c = cl;
    
    if (2 * clauses->len >= clauses->cap) {
        parse_make_clauses(clauses, 0);
    }

    clauses->clauses[clauses->len++] = binding;

    parse_want(toks, TOK_RPAREN);
    if (tok_get(toks) != TOK_RPAREN) {
        parse_bindings(ast, toks, clauses);
    }
}

clauses_t* parse_clause_append (clauses_t* clauses, node_t binding) {
    if (clauses == NULL) {
        clauses = malloc(sizeof(clauses_t));
        clauses->len = 0;
        clauses->cap = 0;
        clauses->clauses = NULL;
        parse_make_clauses(clauses, 4);
        clauses->clauses[clauses->len++] = binding;
        return clauses;
    }

    if (2 * clauses->len >= clauses->cap) {
        parse_make_clauses(clauses, 0);
    }

    clauses->clauses[clauses->len++] = binding;
    return clauses;
}

node_t* parse_multivar (node_t* ast, tokstring_t* toks, tok_t tok) {
    if (__trace__) {
        printf("parse_binary\n");
    }
    node_t* expr = newnode(malloc(sizeof(node_t)));
    expr = parse_expr(expr, toks);
    if (ast->right == NULL) {
        if (__trace__) {
            printf("parse_b_right\n");
        }
        ast->right = expr;
    }
    else {
        //sleep(1);
        if (__trace__) {
            printf("parse_b_growtree\n");
        }
        node_t* op = newnode(malloc(sizeof(node_t)));
        op->c = ast->c;
        op->l = ast->l;
        op->ntype = ast->ntype;
        op->otype = ast->otype;
        op->left = ast;
        op->right = expr;
        ast = op;
        //printf("\n\n\n");
        //parse_printer(ast);
        //printf("\n\n\n");
    }
    if (tok_get(toks) != TOK_RPAREN) {
        return parse_multivar(ast, toks, tok);
    }
    else {
        return ast;
    }
}

void parse_multivar_right (node_t** ast, tokstring_t* toks) {
    if (__trace__) {
        printf("parse_binary\n");
    }
    node_t* expr = newnode(malloc(sizeof(node_t)));
    expr = parse_expr(expr, toks);

    node_t* op = newnode(malloc(sizeof(node_t)));
    op->c = (*ast)->c;
    op->l = (*ast)->l;
    op->ntype = (*ast)->ntype;
    op->etype = (*ast)->etype;
    op->otype = (*ast)->otype;
    op->left = expr;

    node_t* rast = *ast;
    while (rast->right != NULL) {
        rast = rast->right;
    }
    rast->right = op;

    if (tok_get(toks) != TOK_RPAREN) {
        parse_multivar_right(ast, toks);
    }
    else {
        op->right = newnode(malloc(sizeof(node_t)));
        op->right->l = op->l;
        op->right->c = op->c;
        op->right->ntype = NODE_ELIST;
    }
}



void parse_setvalue(node_t* expr, token_t tok, etype_t type) {
    expr->l = tok.l;
    expr->c = tok.c;
    expr->etype = type;
    switch (type) {
    case TYPE_I:
        expr->ival = safe_atoi(tok.val);
        break;
    case TYPE_S:
        expr->sval = tok.val;
        break;
    case TYPE_B:
        expr->bval = (tok.tok == TOK_BVTRUE);
        break;
    default:
        parse_error(expr, "unknown value!");
    }
}

int64_t safe_atoi(char* val) {
    if (val == NULL) {
        return 0;
    }
    int vlen = strlen(val);
    if (vlen == 0) {
        return 0;
    }
    if ((val[0] == '-' && vlen > 20) || vlen > 19) {
        parse_error(NULL, "integer is too big: %s", val);
        return 0;
    }
    int64_t rval = 0;
    int sign = 0;
    int i = 0;
    if (val[i] == '-') {
        i++;
        sign = 1;
    }
    for (; i < vlen && (val[i] != 0); i++) {
        rval += (int64_t) (val[i] - '0');
        if (i < (vlen - 1)) {
            rval *= 10;
        }
    }
    return sign ? -rval : rval;
}

int parse_got (tokstring_t* toks, tok_t tok) {
    if (tok_get(toks) == tok) {
        tok_consume(toks);
        return 1;
    }
    return 0;
}

void parse_want (tokstring_t* toks, tok_t tok) {
    if (!parse_got(toks, tok)) {
        tok_error(toks, "unexpected %s; wanted %s", tok_name(tok_get(toks)), tok_name(tok));
        tok_consume(toks);
    }
}

int tok_get (tokstring_t* toks) {
    return toks->toks[toks->off].tok;
}

token_t token_get (tokstring_t* toks) {
    return toks->toks[toks->off];
}

void tok_consume (tokstring_t* toks) {
    if (__tok_trace__) {
        printf("[%d:%d] %s\n", toks->toks[toks->off].l, toks->toks[toks->off].c, tok_name(toks->toks[toks->off].tok));
    }
    if (toks->off >= toks->len) {
        tok_error(NULL, "cannot consume token: empty token list!");
    }

    if (tok_get(toks) != TOK_STRLIT \
        && tok_get(toks) != TOK_IDENTF
        && tok_get(toks) != TOK_SYMBOL) {
        free(toks->toks[toks->off].val);
    }

    toks->off++;
}

void parse_error (node_t* node, char* msg, ...) {
    va_list v;
    va_start(v, msg);
    if (node == NULL) {
        printf("fatal error: ");
    } 
    else {
        printf("%d:%d: ", node->l, node->c);
    }
    vprintf(msg, v);
    va_end(v);
    //sleep(1);
    printf("\n");
    exit(1);
}

void tok_error (tokstring_t* toks, char* msg, ...) {
    va_list v;
    va_start(v, msg);
    if (toks == NULL) {
        printf("fatal error: ");
    } 
    else {
        printf("%d:%d: ", token_get(toks).l, token_get(toks).c);
    }
    vprintf(msg, v);
    va_end(v);
    //sleep(1);
    printf("\n");
    exit(1);
}

char* type_name (etype_t type) {
    switch (type) {
    case TYPE_B:
        return "boolean";
    case TYPE_S:
        return "string";
    case TYPE_I:
        return "integer";
    case TYPE_U:
        return "undefined";
    default:
        return "unk";
    }
}

int sym_is_string (char** val_) {
    char* val = *val_;
    int rv = 0;
    int __slen = strlen(val);
    int __bsc = 0;
    for (int i = 0; i < __slen; i++) {
        if (val[i] == '\\') {
            if (i > 0 && val[i - 1] != '\\') {
                __bsc++;
            }
        }
    }
    char* __rv = malloc(sizeof(char) * (__slen - __bsc - 1));
    int __rvc = 0;
    if (val[0] != '\"') rv += 1;
    if (val[__slen - 1] != '\"') rv += 1;
    if (val[__slen - 2] == '\\') rv += 1;
    for (int i = 1; i < __slen - 1; i++) {
        if (i < __slen - 2 && val[i] == '\"') rv += 1;
        if (i == '\\' && i < __slen - 2) {
            i++;
            switch (val[i]) {
            case 'n':
                __rv[__rvc++] = '\n';
                break;
            case 't':
                __rv[__rvc++] = '\t';
                break;
            case '\'':
                __rv[__rvc++] = '\'';
                break;
            case '\"':
                __rv[__rvc++] = '\"';
                break;
            case 'r':
                __rv[__rvc++] = '\r';
                break;
            case '\\':
                __rv[__rvc++] = '\\';
                break;
            default:
                rv += 1;
            }
        }
        else {
            __rv[__rvc++] = val[i];
        }
    }
    __rv[__rvc] = 0;
    if (rv > 0) {
        free(__rv);
        __rv = val;
    }
    else {
        free(val);
    }
    *val_ = __rv;
    return rv == 0;
}

int sym_is_bool (char *val) {
    int __slen = strlen(val);
    return (__slen == 2 && val[0] == '#' && (val[1] == 't' || val[1] == 'f'));
}

int sym_is_int (char* val) {
    int __slen = strlen(val);
    if (__slen > 16) return 0;
    for (int i = 0; i < __slen; i++) {
        if (__slen == 1 && i == 0 && val[i] == '-') return 0;
        if (__slen > 1 && i == 0 && val[i] == '-') i++;
        if (!(val[i] >= '0' && val[i] <= '9')) return 0;
    } 
    return 1;
}

void parse_quote (node_t* ast, tokstring_t* toks) {
    if (__trace__) { printf("parse_quote\n"); }
    node_t __newnode;
    newnode(&__newnode);
    __newnode.l = token_get(toks).l;
    __newnode.c = token_get(toks).c;
    switch (tok_get(toks)) {
    case TOK_RPAREN:
        if (__trace__) { printf("parse_quote )\n"); }
        __newnode.ntype = NODE_ELIST;
        ast->clauses = parse_clause_append(ast->clauses, __newnode);
        tok_consume(toks);
        return;
    case TOK_LPAREN:
        if (__trace__) { printf("parse_quote (\n"); }
        __newnode.ntype = NODE_CQUOT;
        tok_consume(toks);
        parse_quote(&__newnode, toks);
        ast->clauses = parse_clause_append(ast->clauses, __newnode);
        break;
    case TOK_PERIOD:
        if (__trace__) { printf("parse_quote .\n"); }
        tok_consume(toks);
        switch (tok_get(toks)) {
        case TOK_LPAREN:
            if (__trace__) { printf("parse_quote (\n"); }
            __newnode.ntype = NODE_CQUOT;
            tok_consume(toks);
            parse_quote(&__newnode, toks);
            ast->clauses = parse_clause_append(ast->clauses, __newnode);
            break;
        case TOK_SYMBOL:
            if (__trace__) { printf("parse_quote %s\n", token_get(toks).val); }
            __newnode.ntype = NODE_LQUOT;
            __newnode.sval = token_get(toks).val;
            tok_consume(toks);
            if (sym_is_string(&__newnode.sval)) {
                /* sval now stores properly formatted string */
                __newnode.ntype = NODE_LQSTR;
                if (__trace__) { printf("parse_quote string %s\n", __newnode.sval); }
            }
            else if (sym_is_bool(__newnode.sval)) {
                __newnode.ntype = NODE_LQFLG;
                __newnode.bval = ((!strcmp(__newnode.sval, "#t")) ? 1 : 0);
                free(__newnode.sval);
            }
            else if (sym_is_int(__newnode.sval)) {
                __newnode.ntype = NODE_LQINT;
                __newnode.ival = safe_atoi(__newnode.sval);
                free(__newnode.sval);
            }
            ast->clauses = parse_clause_append(ast->clauses, __newnode);
            break;
        default:
            parse_error(ast, "fatal quote error");
        }
        parse_want(toks, TOK_RPAREN);
        return;
    case TOK_SYMBOL:
        if (__trace__) { printf("parse_quote %s\n", token_get(toks).val); }
        __newnode.ntype = NODE_LQUOT;
        __newnode.sval = token_get(toks).val;
        tok_consume(toks);
        if (sym_is_string(&__newnode.sval)) {
            /* sval now stores properly formatted string */
            __newnode.ntype = NODE_LQSTR;
            if (__trace__) { printf("parse_quote string %s\n", __newnode.sval); }
        }
        else if (sym_is_bool(__newnode.sval)) {
            __newnode.ntype = NODE_LQFLG;
            __newnode.bval = ((!strcmp(__newnode.sval, "#t")) ? 1 : 0);
            free(__newnode.sval);
        }
        else if (sym_is_int(__newnode.sval)) {
            __newnode.ntype = NODE_LQINT;
            __newnode.ival = safe_atoi(__newnode.sval);
            free(__newnode.sval);
        }
        ast->clauses = parse_clause_append(ast->clauses, __newnode);
        break;
    default:
        parse_error(ast, "fatal quote error");
    }
    parse_quote(ast, toks);
}

node_t* parse_pattern_bind(node_t* arg, node_t* ast, node_t* expr, tokstring_t* toks) {
    node_t* res = newnode(malloc(sizeof(node_t)));
    res->l = ast->l;
    res->c = ast->c;
    switch (ast->ntype) {
    case NODE_ELIST:
    case NODE_BOOLV:
    case NODE_INTEG:
    case NODE_STRNG:
    case NODE_LQUOT:
        free(res);
        res = expr;
        break;
    case NODE_VARBL:
        {
            node_t binding;
            newnode(&binding);
            binding.l = res->l;
            binding.c = res->c;
            binding.ntype = NODE_BINDE;
            binding.sval = ast->sval;
            binding.left = arg;
            res->ntype = NODE_LETEX;
            res->clauses = NULL;
            res->clauses = parse_clause_append(res->clauses, binding);
            res->left = expr;
        }
        break;
    case NODE_UQOPR:
        {
            node_t binding;
            newnode(&binding);
            binding.l = ast->l;
            binding.c = ast->c;
            binding.ntype = NODE_BINDE;
            binding.sval = pgensym(calloc(30, sizeof(char)), "*unquot");

            node_t* unquote = newnode(malloc(sizeof(node_t)));
            unquote->l = ast->l;
            unquote->c = ast->c;
            unquote->ntype = NODE_UQOPR;
            unquote->left = arg;
            binding.left = unquote;

            node_t* varbl = newnode(malloc(sizeof(node_t)));
            varbl->l = ast->l;
            varbl->c = ast->c;
            varbl->ntype = NODE_VARBL;
            varbl->sval = binding.sval;

            res->ntype = NODE_LETEX;
            res->clauses = NULL;
            res->clauses = parse_clause_append(res->clauses, binding);
            res->left = parse_pattern_bind(varbl, ast->left, expr, toks);  
        }
        break;
    case NODE_IUNOP:
        {
            switch (ast->otype) {
            case OP_BOX:
                {                    
                    node_t* unbox = newnode(malloc(sizeof(node_t)));
                    node_t binding;
                    newnode(&binding);
                    binding.l = ast->l;
                    binding.c = ast->c;
                    unbox->l = ast->l;
                    unbox->c = ast->c;
                    binding.ntype = NODE_BINDE;
                    char* sym = calloc(30, sizeof(char));
                    binding.sval = pgensym(sym, "*box");
                    unbox->ntype = NODE_IUNOP;
                    unbox->otype = OP_UBX;
                    unbox->left = arg;
                    binding.left = unbox;
                    res->ntype = NODE_LETEX;
                    res->clauses = NULL;
                    node_t* varbl = newnode(malloc(sizeof(node_t)));
                    varbl->l = ast->l;
                    varbl->c = ast->c;
                    varbl->ntype = NODE_VARBL;
                    varbl->sval = sym;
                    res->clauses = parse_clause_append(res->clauses, binding);
                    res->left = parse_pattern_bind(varbl, ast->left, expr, toks);
                }
                break;
            default:
                parse_error(ast, "unknown pattern binding");
            }
        }
        break;
    case NODE_IBNOP:
        {
            switch (ast->otype) {
            case OP_CNS:
                {                    
                    node_t* car = newnode(malloc(sizeof(node_t)));
                    node_t* cdr = newnode(malloc(sizeof(node_t)));
                    node_t binding1;
                    newnode(&binding1);
                    node_t binding2;
                    newnode(&binding2);
                    binding1.l = ast->l;
                    binding1.c = ast->c;
                    car->l = ast->l;
                    car->c = ast->c;
                    binding1.ntype = NODE_BINDE;
                    char* sym1 = calloc(30, sizeof(char));
                    binding1.sval = pgensym(sym1, "*car");
                    car->ntype = NODE_IUNOP;
                    car->otype = OP_CAR;
                    car->left = arg;
                    binding1.left = car;
                    binding2.l = ast->l;
                    binding2.c = ast->c;
                    cdr->l = ast->l;
                    cdr->c = ast->c;
                    binding2.ntype = NODE_BINDE;
                    char* sym2 = calloc(30, sizeof(char));
                    binding2.sval = pgensym(sym2, "*cdr");
                    cdr->ntype = NODE_IUNOP;
                    cdr->otype = OP_CDR;
                    cdr->left = arg;
                    binding2.left = cdr;
                    res->ntype = NODE_LETEX;
                    res->clauses = NULL;
                    node_t* varbl1 = newnode(malloc(sizeof(node_t)));
                    varbl1->l = ast->l;
                    varbl1->c = ast->c;
                    varbl1->ntype = NODE_VARBL;
                    varbl1->sval = sym1;
                    node_t* varbl2 = newnode(malloc(sizeof(node_t)));
                    varbl2->l = ast->l;
                    varbl2->c = ast->c;
                    varbl2->ntype = NODE_VARBL;
                    varbl2->sval = sym2;
                    res->clauses = parse_clause_append(res->clauses, binding1);
                    res->clauses = parse_clause_append(res->clauses, binding2);
                    res->left = parse_pattern_bind(varbl1, ast->left, parse_pattern_bind(varbl2, ast->right, expr, toks), toks);
                }
                break;
            default:
                parse_error(ast, "unknown pattern binding");
            }
        }
        break;
    case NODE_QMARK:
        {
            free(res);
            res = parse_pattern_bind(arg, ast->right, expr, toks);
        }
        break;
    default:
        parse_error(ast, "unknown pattern binding!");
    }

    return res;
}

/* converts pattern to condition */
node_t* parse_pattern(node_t* arg, node_t* ast, tokstring_t* toks) {
    node_t* cond = newnode(malloc(sizeof(node_t)));
    cond->l = ast->l;
    cond->c = ast->c;
    switch (ast->ntype) {
    case NODE_ELIST:
        {
            node_t* rval = newnode(malloc(sizeof(node_t)));
            rval->l = ast->l;
            rval->c = ast->c;
            rval->ntype = NODE_ELIST;
            rval->bval = ast->bval;
            cond->left = arg;
            cond->right = rval;
            cond->ntype = NODE_IBNOP;
            cond->otype = CMP_EQ;
        }
        break;
    case NODE_BOOLV:
        {
            node_t* rval = newnode(malloc(sizeof(node_t)));
            rval->l = ast->l;
            rval->c = ast->c;
            rval->ntype = NODE_BOOLV;
            rval->bval = ast->bval;
            cond->left = arg;
            cond->right = rval;
            cond->ntype = NODE_IBNOP;
            cond->otype = CMP_EQ;
        }
        break;
    case NODE_INTEG:
        {
            node_t* rval = newnode(malloc(sizeof(node_t)));
            rval->l = ast->l;
            rval->c = ast->c;
            rval->ntype = NODE_INTEG;
            rval->ival = ast->ival;
            cond->left = arg;
            cond->right = rval;
            cond->ntype = NODE_IBNOP;
            cond->otype = CMP_EQ;
            //parse_printer(cond);
            //sleep(1);
        }
        break;
    case NODE_STRNG:
        {
            node_t* strchk = newnode(malloc(sizeof(node_t)));
            node_t* streql = newnode(malloc(sizeof(node_t)));
            node_t* strval = newnode(malloc(sizeof(node_t)));
            strchk->l = ast->l;
            strchk->c = ast->c;
            strchk->ntype = NODE_IUNOP;
            strchk->otype = CHKSTR;
            strchk->left = arg;
            streql->l = ast->l;
            streql->c = ast->c;
            streql->ntype = NODE_IBNOP;
            streql->otype = CMP_RE;
            strval->l = ast->l;
            strval->c = ast->c;
            strval->ntype = NODE_STRNG;
            strval->sval = ast->sval;
            streql->left = strval;
            streql->right = arg;
            cond->sval = NULL;
            cond->ntype = NODE_IUNOP;
            cond->otype = OP_AND;
            cond->clauses = NULL;
            cond->clauses = parse_clause_append(cond->clauses, *strchk);
            cond->clauses = parse_clause_append(cond->clauses, *streql);
            free(strchk);
            free(streql);
        }
        break;
    case NODE_LQUOT:
        if (ast->sval != NULL) {
            node_t* symchk = newnode(malloc(sizeof(node_t)));
            node_t* symeql = newnode(malloc(sizeof(node_t)));
            node_t* symval = newnode(malloc(sizeof(node_t)));
            symchk->l = ast->l;
            symchk->c = ast->c;
            symchk->ntype = NODE_IUNOP;
            symchk->otype = CHKSYM;
            symchk->left = arg;
            symeql->l = ast->l;
            symeql->c = ast->c;
            symeql->ntype = NODE_IBNOP;
            symeql->otype = CMP_SE;
            symval->l = ast->l;
            symval->c = ast->c;
            symval->ntype = NODE_LQUOT;
            symval->sval = ast->sval;
            symeql->left = symval;
            symeql->right = arg;
            cond->sval = NULL;
            cond->ntype = NODE_IUNOP;
            cond->otype = OP_AND;
            cond->clauses = NULL;
            cond->clauses = parse_clause_append(cond->clauses, *symchk);
            cond->clauses = parse_clause_append(cond->clauses, *symeql);
            free(symchk);
            free(symeql);
        }
        break;
    case NODE_VARBL:
        {
            cond->bval = 1;
            cond->ntype = NODE_BOOLV;
        }
        break;
    case NODE_UQOPR:
        {
            node_t symchk;
            newnode(&symchk);
            symchk.l = ast->l;
            symchk.c = ast->c;
            symchk.ntype = NODE_IUNOP;
            symchk.otype = CHKSYM;
            symchk.left = arg;

            node_t let;
            newnode(&let);
            let.l = ast->l;
            let.c = ast->c;
            let.ntype = NODE_LETEX;

            node_t binding;
            newnode(&binding);
            binding.l = ast->l;
            binding.c = ast->c;
            binding.ntype = NODE_BINDE;
            binding.sval = pgensym(calloc(30, sizeof(char)), "*unquot");

            node_t* unquote = newnode(malloc(sizeof(node_t)));
            unquote->l = ast->l;
            unquote->c = ast->c;
            unquote->ntype = NODE_UQOPR;
            unquote->left = arg;
            binding.left = unquote;

            let.clauses = NULL;
            let.clauses = parse_clause_append(let.clauses, binding);

            node_t* varbl = newnode(malloc(sizeof(node_t)));
            varbl->l = ast->l;
            varbl->c = ast->c;
            varbl->ntype = NODE_VARBL;
            varbl->sval = binding.sval;
            let.left = parse_pattern(varbl, ast->left, toks);

            cond->ntype = NODE_IUNOP;
            cond->otype = OP_AND;
            cond->clauses = NULL;
            cond->clauses = parse_clause_append(cond->clauses, symchk);
            cond->clauses = parse_clause_append(cond->clauses, let);  
        }
        break;
    case NODE_IUNOP:
        {
            switch (ast->otype) {
            case OP_BOX:
                {
                    node_t chkbox;
                    newnode(&chkbox);
                    chkbox.l = ast->l;
                    chkbox.c = ast->c;
                    chkbox.ntype = NODE_IUNOP;
                    chkbox.otype = CHKBOX;
                    chkbox.left = arg;
                    
                    node_t let; 
                    newnode(&let);
                    node_t* unbox = newnode(malloc(sizeof(node_t)));
                    node_t binding;
                    newnode(&binding);
                    binding.l = ast->l;
                    binding.c = ast->c;
                    let.l = ast->l;
                    let.c = ast->c;
                    unbox->l = ast->l;
                    unbox->c = ast->c;
                    binding.ntype = NODE_BINDE;
                    char* sym = calloc(30, sizeof(char));
                    binding.sval = pgensym(sym, "*box");
                    unbox->ntype = NODE_IUNOP;
                    unbox->otype = OP_UBX;
                    unbox->left = arg;
                    binding.left = unbox;
                    let.ntype = NODE_LETEX;
                    let.clauses = NULL;
                    let.clauses = parse_clause_append(let.clauses, binding);
                    node_t* varbl = malloc(sizeof(node_t));
                    newnode(varbl);
                    varbl->l = let.l;
                    varbl->c = let.c;
                    varbl->ntype = NODE_VARBL;
                    varbl->sval = sym;
                    let.left = parse_pattern(varbl, ast->left, toks);

                    cond->ntype = NODE_IUNOP;
                    cond->otype = OP_AND;
                    cond->clauses = NULL;
                    cond->clauses = parse_clause_append(cond->clauses, chkbox);
                    cond->clauses = parse_clause_append(cond->clauses, let);
                }
                break;
            default:
                parse_error(ast, "unknown pattern!");
            }
        }
        break;
    case NODE_IBNOP:
        {
            switch (ast->otype) {
            case OP_CNS:
                {
                    node_t chkcns;
                    newnode(&chkcns);
                    chkcns.l = ast->l;
                    chkcns.c = ast->c;
                    chkcns.ntype = NODE_IUNOP;
                    chkcns.otype = CHKCNS;
                    chkcns.left = arg;
                    
                    node_t let; 
                    newnode(&let);
                    node_t* car = newnode(malloc(sizeof(node_t)));
                    node_t* cdr = newnode(malloc(sizeof(node_t)));
                    node_t binding1;
                    node_t binding2;
                    newnode(&binding1);
                    binding1.l = ast->l;
                    binding1.c = ast->c;
                    let.l = ast->l;
                    let.c = ast->c;
                    car->l = ast->l;
                    car->c = ast->c;
                    binding1.ntype = NODE_BINDE;
                    char* sym1 = calloc(30, sizeof(char));
                    char* sym2 = calloc(30, sizeof(char));
                    binding1.sval = pgensym(sym1, "*car");
                    car->ntype = NODE_IUNOP;
                    car->otype = OP_CAR;
                    car->left = arg;
                    binding1.left = car;
                    newnode(&binding2);
                    binding2.l = ast->l;
                    binding2.c = ast->c;
                    cdr->l = ast->l;
                    cdr->c = ast->c;
                    binding2.ntype = NODE_BINDE;
                    binding2.sval = pgensym(sym2, "*cdr");
                    cdr->ntype = NODE_IUNOP;
                    cdr->otype = OP_CDR;
                    cdr->left = arg;
                    binding2.left = cdr;
                    let.ntype = NODE_LETEX;
                    let.clauses = NULL;
                    let.clauses = parse_clause_append(let.clauses, binding1);
                    let.clauses = parse_clause_append(let.clauses, binding2);
                    node_t *varbl1 = malloc(sizeof(node_t));
                    newnode(varbl1);
                    varbl1->l = let.l;
                    varbl1->c = let.c;
                    varbl1->ntype = NODE_VARBL;
                    varbl1->sval = sym1;
                    node_t *varbl2 = malloc(sizeof(node_t));
                    newnode(varbl2);
                    varbl2->l = let.l;
                    varbl2->c = let.c;
                    varbl2->ntype = NODE_VARBL;
                    varbl2->sval = sym2;

                    node_t *andl = parse_pattern(varbl1, ast->left, toks);
                    node_t *andr = parse_pattern(varbl2, ast->right, toks);

                    //sleep(1);
                    //printf("\n\n\nTESTER:\n");
                    //parse_printer(andl);
                    //printf("\n");
                    //parse_printer(andr);
                    //sleep(1);

                    node_t* and = newnode(malloc(sizeof(node_t)));
                    and->l = ast->l;
                    and->c = ast->c;
                    and->ntype = NODE_IUNOP;
                    and->otype = OP_AND;
                    and->clauses = NULL;
                    and->clauses = parse_clause_append(and->clauses, *andl);
                    and->clauses = parse_clause_append(and->clauses, *andr);
                    //free(andl);
                    //free(andr);
                    let.left = and;

                    //printf("\n\n\nTESTER 2:\n");
                    //parse_printer(and);
                    //printf("\n");
                    //sleep(1);
                    //parse_printer(&let);
                    //printf("\n");
                    //sleep(1);

                    cond->ntype = NODE_IUNOP;
                    cond->otype = OP_AND;
                    cond->clauses = NULL;
                    cond->clauses = parse_clause_append(cond->clauses, chkcns);
                    cond->clauses = parse_clause_append(cond->clauses, let);

                    //printf("\n\n\nTESTER 3:\n");
                    //parse_printer(cond);
                    //sleep(1);
                }
                break;
            default:
                parse_error(ast, "unknown pattern!");
            }
        }
        break;
    case NODE_QMARK:
        {
            if (ast->left->ntype == NODE_IUNOP) {
                cond = ast->left;
                cond->left = arg;
            }
            else {
                cond = ast->left;
                cond->clauses = parse_clause_append(cond->clauses, *arg);
            }
        }
        break;
    default:
        parse_error(ast, "invalid pattern!");
    }

    return cond;
}

void parse_quasiquote (node_t* ast, tokstring_t* toks) {
    if (__trace__) { printf("parse_quasiquote\n"); }
    node_t __newnode;
    newnode(&__newnode);
    __newnode.l = token_get(toks).l;
    __newnode.c = token_get(toks).c;
    switch (tok_get(toks)) {
    case TOK_RPAREN:
        if (__trace__) { printf("parse_quasiquote )\n"); }
        __newnode.ntype = NODE_ELIST;
        ast->clauses = parse_clause_append(ast->clauses, __newnode);
        tok_consume(toks);
        return;
    case TOK_LPAREN:
        if (__trace__) { printf("parse_quasiquote (\n"); }
        __newnode.ntype = NODE_CQUOT;
        tok_consume(toks);
        parse_quasiquote(&__newnode, toks);
        ast->clauses = parse_clause_append(ast->clauses, __newnode);
        break;
    case TOK_PERIOD:
        if (__trace__) { printf("parse_quasiquote .\n"); }
        tok_consume(toks);
        switch (tok_get(toks)) {
        case TOK_LPAREN:
            if (__trace__) { printf("parse_quasiquote (\n"); }
            __newnode.ntype = NODE_CQUOT;
            tok_consume(toks);
            parse_quasiquote(&__newnode, toks);
            ast->clauses = parse_clause_append(ast->clauses, __newnode);
            break;
        case TOK_SYMBOL:
            if (__trace__) { printf("parse_quasiquote %s\n", token_get(toks).val); }
            __newnode.ntype = NODE_LQUOT;
            __newnode.sval = token_get(toks).val;
            tok_consume(toks);
            if (sym_is_string(&__newnode.sval)) {
                /* sval now stores properly formatted string */
                __newnode.ntype = NODE_LQSTR;
                if (__trace__) { printf("parse_quasiquote string %s\n", __newnode.sval); }
            }
            else if (sym_is_bool(__newnode.sval)) {
                __newnode.ntype = NODE_LQFLG;
                __newnode.bval = ((!strcmp(__newnode.sval, "#t")) ? 1 : 0);
                free(__newnode.sval);
            }
            else if (sym_is_int(__newnode.sval)) {
                __newnode.ntype = NODE_LQINT;
                __newnode.ival = safe_atoi(__newnode.sval);
                free(__newnode.sval);
            }
            ast->clauses = parse_clause_append(ast->clauses, __newnode);
            break;
        default:
            parse_error(ast, "fatal quasiquote error");
        }
        parse_want(toks, TOK_RPAREN);
        return;
    case TOK_SYMBOL:
        if (__trace__) { printf("parse_quasiquote %s\n", token_get(toks).val); }
        __newnode.ntype = NODE_LQUOT;
        __newnode.sval = token_get(toks).val;
        tok_consume(toks);
        if (sym_is_string(&__newnode.sval)) {
            /* sval now stores properly formatted string */
            __newnode.ntype = NODE_LQSTR;
            if (__trace__) { printf("parse_quasiquote string %s\n", __newnode.sval); }
        }
        else if (sym_is_bool(__newnode.sval)) {
            __newnode.ntype = NODE_LQFLG;
            __newnode.bval = ((!strcmp(__newnode.sval, "#t")) ? 1 : 0);
            free(__newnode.sval);
        }
        else if (sym_is_int(__newnode.sval)) {
            __newnode.ntype = NODE_LQINT;
            __newnode.ival = safe_atoi(__newnode.sval);
            free(__newnode.sval);
        }
        ast->clauses = parse_clause_append(ast->clauses, __newnode);
        break;
    case TOK_UNQUOT:
        if (__trace__) { printf("parse_quasiquote unquote\n"); }
        {
            __newnode.ntype = NODE_QTOPR;
            __newnode.left = newnode(malloc(sizeof(node_t)));
            tok_consume(toks);
            __newnode.left = parse_expr(__newnode.left, toks);
            ast->clauses = parse_clause_append(ast->clauses, __newnode);
        }
        break;
    default:
        parse_error(ast, "fatal quasiquote error");
    }
    parse_quasiquote(ast, toks);
}

node_t* parse_match (node_t* ast) {
    if (ast == NULL) return ast;
    switch (ast->ntype) {
    case NODE_BEGIN:
        if (ast->clauses != NULL) {
            for (int i = 0; i < ast->clauses->len; i++) {
                node_t* __node = &(ast->clauses->clauses[i]);
                __node = parse_match(__node);
                ast->clauses->clauses[i] = *__node;
            }
        }
        break;
    case NODE_INTEG:
    case NODE_BOOLV:
    case NODE_DEFLT:
    case NODE_STRNG:
    case NODE_VARBL:
    case NODE_ELIST:
    case NODE_QMARK:
    case NODE_LQINT:
    case NODE_LQSTR:
    case NODE_LQFLG:
    case NODE_FLCLS:
        break;
    case NODE_UQOPR:
    case NODE_QTOPR:
    case NODE_DEFFN:
    case NODE_BINDE:
    case NODE_CLOSE:
    case NODE_FLOPN:
    case NODE_FLWRT:
        ast->left = parse_match(ast->left);
        break;
    case NODE_LQUOT:
    case NODE_CQUOT:
        if (ast->clauses != NULL) {
            for (int i = 0; i < ast->clauses->len; i++) {
                node_t* __node = &(ast->clauses->clauses[i]);
                __node = parse_match(__node);
                ast->clauses->clauses[i] = *__node;
            }
        }
        break;
    case NODE_LETEX:
    case NODE_IUNOP:
        if (ast->clauses != NULL) {
            for (int i = 0; i < ast->clauses->len; i++) {
                node_t* __node = &(ast->clauses->clauses[i]);
                __node = parse_match(__node);
                ast->clauses->clauses[i] = *__node;
            }
        }
        if (ast->left != NULL) {
            ast->left = parse_match(ast->left);
        }
        break;
    case NODE_GLOBL:
        if (ast->left != NULL) {
            ast->left = parse_match(ast->left);
        }
        break;
    case NODE_MUTST:
        ast->left = parse_match(ast->left);
        break;
    case NODE_IBNOP:
    case NODE_MTCHC:
    case NODE_MULTI:
    case NODE_CONDC:
        ast->left = parse_match(ast->left);
        ast->right = parse_match(ast->right);
        break;
    case NODE_IFEXP:
        ast->cond = parse_match(ast->cond);
        ast->left = parse_match(ast->left);
        ast->right = parse_match(ast->right);
        break;
    case NODE_CONDE:
        if (ast->clauses != NULL) {
            for (int i = 0; i < ast->clauses->len; i++) {
                node_t* __node = &(ast->clauses->clauses[i]);
                __node = parse_match(__node);
                ast->clauses->clauses[i] = *__node;
            }
        }
        ast->cond = parse_match(ast->cond);
        break;
    case NODE_CALLF:
        if (ast->clauses != NULL) {
            for (int i = 0; i < ast->clauses->len; i++) {
                node_t* __node = &(ast->clauses->clauses[i]);
                __node = parse_match(__node);
                ast->clauses->clauses[i] = *__node;
            }
        }
        if (ast->cond != NULL) {
            ast->cond = parse_match(ast->cond);
        }
        break;
    case NODE_MATCH:
        /* the interesting case. */
        /* we first eliminate all submatches. */
        //parse_printer(ast);
        if (ast->clauses != NULL) {
            for (int i = 0; i < ast->clauses->len; i++) {
                node_t* __node = &(ast->clauses->clauses[i]);
                __node = parse_match(__node);
                ast->clauses->clauses[i] = *__node;
            }
        }
        ast->left = parse_match(ast->left);
        //parse_printer(ast);
        //parse_printer(ast);
        /* next we perform the transformations */
        {
            node_t* newast = newnode(malloc(sizeof(node_t)));
            newast->l = ast->l;
            newast->c = ast->c;
            newast->ntype = NODE_LETEX;
            
            node_t binding;
            newnode(&binding);
            binding.l = ast->l;
            binding.c = ast->c;
            binding.sval = pgensym(calloc(30, sizeof(char)), "*match");
            binding.left = ast->left;
            binding.ntype = NODE_BINDE;

            node_t* varbl = newnode(malloc(sizeof(node_t)));
            varbl->l = binding.l;
            varbl->c = binding.c;
            varbl->ntype = NODE_VARBL;
            varbl->sval = binding.sval;

            newast->clauses = NULL;
            newast->clauses = parse_clause_append(newast->clauses, binding);

            node_t* newcond = newnode(malloc(sizeof(node_t)));
            newcond->l = ast->l;
            newcond->c = ast->c;
            newcond->ntype = NODE_CONDE;
            newcond->clauses = NULL;
            if (ast->clauses == NULL) {
                parse_error(ast, "match must accept at least one condition!");
            }
            for (int i = 0; i < ast->clauses->len; i++) {
                node_t clause = ast->clauses->clauses[i];
                if (clause.left->ntype == NODE_DEFLT) {
                    /* make our else */
                    newcond->cond = clause.right;
                    //printf("\n");
                    //sleep(1);
                    //parse_printer(clause.right);
                    //sleep(1);
                    //printf("passing?\n");
                    break;
                }
                else {
                    node_t* pat = clause.left;
                    clause.left = parse_pattern(varbl, pat, NULL);
                    clause.right = parse_pattern_bind(varbl, pat, clause.right, NULL);
                    clause.ntype = NODE_CONDC;
                    newcond->clauses = parse_clause_append(newcond->clauses, clause);
                }
            }
            if (newcond->cond == NULL) {
                node_t* newelse = newnode(malloc(sizeof(node_t)));
                newelse->l = (newelse->c = 0);
                newelse->ntype = NODE_IUNOP;
                newelse->otype = OP_INC;
                node_t* newfalse = newnode(malloc(sizeof(node_t)));
                newfalse->l = (newfalse->c = 0);
                newfalse->bval = 0;
                newfalse->ntype = NODE_BOOLV;
                newelse->left = newfalse;
                newcond->cond = newelse;
            }

            /* put it all together */
            //printf("did it work?\n");
            newast->left = newcond;
            ast = newast;
            //sleep(1);
            //parse_printer(ast);
            //sleep(1);
            //printf("\npogchamp\n");
        }
        break;
    default:
        parse_error(ast, "fatal error: parse_match is not comprehensive! (%d)", ast->ntype);
    }

    return ast;
}

void parse_printer (node_t* ast) {
    if (ast == NULL) { printf("woops.\n"); return; }
    switch (ast->ntype) {
    case NODE_BEGIN:
        if (ast->clauses != NULL) {
            for (int i = 0; i < ast->clauses->len; i++) {
                parse_printer(&(ast->clauses->clauses[i]));
                printf("\n");
            }
        }
        break;
    case NODE_INTEG:
        printf("%li ", ast->ival);
        break;
    case NODE_BOOLV:
        printf("%s ", (ast->bval) ? "#t" : "#f");
        break;
    case NODE_DEFLT:
        printf("_ ");
        break;
    case NODE_STRNG:
        printf("\"%s\" ", ast->sval);
        break;
    case NODE_VARBL:
        printf("%s ", ast->sval);
        break;
    case NODE_ELIST:
        printf("() ");
        break;
    case NODE_FLOPN:
        printf("(file-open ");
        parse_printer(ast->left);
        printf(") ");
        break;
    case NODE_FLWRT:
        printf("(file-write ");
        parse_printer(ast->left);
        printf(") ");
        break;
    case NODE_FLCLS:
        printf("(file-close ");
        printf(") ");
        break;
    case NODE_QMARK:
        if (ast->left->ntype == NODE_IUNOP) {
            printf("(? %s ", optostring(ast->left->otype));
        }
        else {
            printf("(? %s ", ast->left->sval);
        }
        if (ast->right != NULL) {
            printf("%s ", ast->right->sval);
        }
        printf(")");
        break;
    case NODE_LQINT:
        printf("\'%li ", ast->ival);
        break;
    case NODE_LQSTR:
        printf("\'\"%s\" ", ast->sval);
        break;
    case NODE_LQFLG:
        printf("\'%s ", (ast->bval) ? "#t" : "#f");
        break;
    case NODE_UQOPR:
    case NODE_QTOPR:
        printf(",");
        parse_printer(ast->left);
        break;
    case NODE_BINDE:
        printf("(%s ", ast->sval);
        parse_printer(ast->left);
        printf(")");
        break;
    case NODE_DEFFN:
        printf("(define (%s ", ast->sval);
        if (ast->clauses != NULL) {
            for (int i = 0; i < ast->clauses->len; i++) {
                printf("%s ", ast->clauses->clauses[i].sval);
            }
        }
        printf(") ");
        parse_printer(ast->left);
        printf(")\n\n");
        break;
    case NODE_CLOSE:
        printf("(lambda (");
        for (int i = 0; i < ast->clauses->len; i++) {
            printf("%s ", ast->clauses->clauses[i].sval);
        }
        printf(") ");
        parse_printer(ast->left);
        printf(") ");
        break;
    case NODE_LQUOT:
        printf("\'%s ", ast->sval);
        break;
    case NODE_CQUOT:
        printf("\'");
        if (ast->clauses != NULL) {
            printf("(");
            for (int i = 0; i < ast->clauses->len; i++) {
                parse_printer(&(ast->clauses->clauses[i]));
            }
            printf(") ");
        }
        break;
    case NODE_LETEX:
        printf("(let (");
        if (ast->clauses != NULL) {
            for (int i = 0; i < ast->clauses->len; i++) {
                parse_printer(&(ast->clauses->clauses[i]));
            }
        }
        printf(") ");
        parse_printer(ast->left);
        printf(") ");
        break;
    case NODE_IUNOP:
        printf("(%s ", optostring(ast->otype));
        if (ast->clauses != NULL) {
            for (int i = 0; i < ast->clauses->len; i++) {
                parse_printer(&(ast->clauses->clauses[i]));
            }
        }
        if (ast->left != NULL) {
            parse_printer(ast->left);
        }
        printf(") ");
        break;
    case NODE_IBNOP:
        printf("(%s ", optostring(ast->otype));
        parse_printer(ast->left);
        parse_printer(ast->right);
        printf(") ");
        break;
    case NODE_MULTI:
        parse_printer(ast->left);
        printf("#;\n");
        parse_printer(ast->right);
        break;
    case NODE_MTCHC:
    case NODE_CONDC:
        printf("[");
        parse_printer(ast->left);
        parse_printer(ast->right);
        printf("] ");
        break;
    case NODE_IFEXP:
        printf("(if ");
        parse_printer(ast->cond);
        parse_printer(ast->left);
        parse_printer(ast->right);
        printf(") ");
        break;
    case NODE_CONDE:
        printf("(cond (");
        if (ast->clauses != NULL) {
            for (int i = 0; i < ast->clauses->len; i++) {
                parse_printer(&(ast->clauses->clauses[i]));
            }
        }
        printf("[else ");
        parse_printer(ast->cond);
        printf("] ) ) ");
        break;
    case NODE_CALLF:
        if (ast->cond != NULL) {
            printf("(");
            parse_printer(ast->cond);
        }
        else {
            printf("(%s ", ast->sval);
        }
        if (ast->clauses != NULL) {
            for (int i = 0; i < ast->clauses->len; i++) {
                parse_printer(&(ast->clauses->clauses[i]));
            }
        }
        printf(") ");
        break;
    case NODE_MATCH:
        printf("(match ");
        parse_printer(ast->left);
        if (ast->clauses != NULL) {
            for (int i = 0; i < ast->clauses->len; i++) {
                parse_printer(&(ast->clauses->clauses[i]));
            }
        }
        printf(") ");
        break;
    case NODE_GLOBL:
        printf("(define %s ", ast->sval);
        parse_printer(ast->left);
        printf(")\n");
        break;
    case NODE_MUTST:
        printf("(set! %s ", ast->sval);
        parse_printer(ast->left);
        printf(")");
        break;
    default:
        parse_error(ast, "fatal error: parse_printer is not comprehensive! (%d)", ast->ntype);
    }
}

node_t* parse_andor (node_t* ast) {
    switch (ast->ntype) {
    case NODE_BEGIN:
        if (ast->clauses != NULL) {
            for (int i = 0; i < ast->clauses->len; i++) {
                node_t* __node = &(ast->clauses->clauses[i]);
                __node = parse_andor(__node);
                ast->clauses->clauses[i] = *__node;
            }
        }
        break;
    case NODE_INTEG:
    case NODE_BOOLV:
    case NODE_STRNG:
    case NODE_VARBL:
    case NODE_ELIST:
    case NODE_LQINT:
    case NODE_LQSTR:
    case NODE_LQFLG:
    case NODE_FLCLS:
        break;
    case NODE_UQOPR:
    case NODE_QTOPR:
    case NODE_BINDE:
    case NODE_DEFFN:
    case NODE_CLOSE:
    case NODE_FLOPN:
    case NODE_FLWRT:
        ast->left = parse_andor(ast->left);
        break;
    case NODE_LQUOT:
    case NODE_CQUOT:
        if (ast->clauses != NULL) {
            for (int i = 0; i < ast->clauses->len; i++) {
                node_t* __node = &(ast->clauses->clauses[i]);
                __node = parse_andor(__node);
                ast->clauses->clauses[i] = *__node;
            }
        }
        break;
    case NODE_LETEX:
        if (ast->clauses != NULL) {
            for (int i = 0; i < ast->clauses->len; i++) {
                node_t* __node = &(ast->clauses->clauses[i]);
                __node = parse_andor(__node);
                ast->clauses->clauses[i] = *__node;
            }
        }
        if (ast->left != NULL) {
            ast->left = parse_andor(ast->left);
        }
        break;
    case NODE_IUNOP:
        if (ast->clauses != NULL) {
            for (int i = 0; i < ast->clauses->len; i++) {
                node_t* __node = &(ast->clauses->clauses[i]);
                __node = parse_andor(__node);
                ast->clauses->clauses[i] = *__node;
            }
        }
        if (ast->left != NULL) {
            ast->left = parse_andor(ast->left);
        }

        if (ast->otype == OP_AND) {
            /* the interesting case */
            node_t* nfalse = newnode(malloc(sizeof(node_t)));
            node_t* ntrue = newnode(malloc(sizeof(node_t)));
            nfalse->l = (nfalse->c = (ntrue->l = (ntrue->c = 0)));
            nfalse->ntype = (ntrue->ntype = NODE_BOOLV);
            nfalse->bval = 0;
            ntrue->bval = 1;

            node_t* andif = newnode(malloc(sizeof(node_t)));
            andif->l = ast->l;
            andif->c = ast->c;
            andif->ntype = NODE_IFEXP;
            andif->cond = &(ast->clauses->clauses[0]);
            andif->left = ntrue;
            andif->right = nfalse;
            node_t* andifcur = andif;
            node_t* nandif;
            for (int i = 1; i < ast->clauses->len; i++) {
                nandif = newnode(malloc(sizeof(node_t)));
                nandif->l = ast->l;
                nandif->c = ast->c;
                nandif->ntype = NODE_IFEXP;
                nandif->cond = &(ast->clauses->clauses[i]);
                nandif->left = ntrue;
                nandif->right = nfalse;
                andifcur->left = nandif;
                andifcur = nandif;
            }
            ast = andif;
        }
        else if (ast->otype == OP_LOR) {
            /* the interesting case */
            node_t* nfalse = newnode(malloc(sizeof(node_t)));
            node_t* ntrue = newnode(malloc(sizeof(node_t)));
            nfalse->l = (nfalse->c = (ntrue->l = (ntrue->c = 0)));
            nfalse->ntype = (ntrue->ntype = NODE_BOOLV);
            nfalse->bval = 0;
            ntrue->bval = 1;

            node_t* andif = newnode(malloc(sizeof(node_t)));
            andif->l = ast->l;
            andif->c = ast->c;
            andif->ntype = NODE_IFEXP;
            andif->cond = &(ast->clauses->clauses[0]);
            andif->left = ntrue;
            andif->right = nfalse;
            node_t* andifcur = andif;
            node_t* nandif;
            for (int i = 1; i < ast->clauses->len; i++) {
                nandif = newnode(malloc(sizeof(node_t)));
                nandif->l = ast->l;
                nandif->c = ast->c;
                nandif->ntype = NODE_IFEXP;
                nandif->cond = &(ast->clauses->clauses[i]);
                nandif->left = ntrue;
                nandif->right = nfalse;
                andifcur->right = nandif;
                andifcur = nandif;
            }
            ast = andif;
        }
        break;
    case NODE_IBNOP:
    case NODE_CONDC:
    case NODE_MULTI:
        ast->left = parse_andor(ast->left);
        ast->right = parse_andor(ast->right);
        break;
    case NODE_IFEXP:
        ast->cond = parse_andor(ast->cond);
        ast->left = parse_andor(ast->left);
        ast->right = parse_andor(ast->right);
        break;
    case NODE_CONDE:
        if (ast->clauses != NULL) {
            for (int i = 0; i < ast->clauses->len; i++) {
                node_t* __node = &(ast->clauses->clauses[i]);
                __node = parse_andor(__node);
                ast->clauses->clauses[i] = *__node;
            }
        }
        ast->cond = parse_andor(ast->cond);
        break;
    case NODE_CALLF:
        if (ast->clauses != NULL) {
            for (int i = 0; i < ast->clauses->len; i++) {
                node_t* __node = &(ast->clauses->clauses[i]);
                __node = parse_andor(__node);
                ast->clauses->clauses[i] = *__node;
            }
        }
        if (ast->cond != NULL) {
            ast->cond = parse_andor(ast->cond);
        }
        break;
    case NODE_GLOBL:
        if (ast->left != NULL) {
            ast->left = parse_andor(ast->left);
        }
        break;
    case NODE_MUTST:
        ast->left = parse_andor(ast->left);
        break;
    case NODE_MATCH:
    case NODE_MTCHC:
    case NODE_DEFLT:
    case NODE_QMARK:
        parse_error(ast, "fatal error: parse_match special nodes not fully removed!");
        break;   
    default:
        parse_error(ast, "fatal error: parse_match is not comprehensive! (%d)", ast->ntype);
    }

    return ast;
}

char* optostring(otype_t op) {
    switch (op) {
    case OP_UND:
        return "<unk-op>";
    case OP_ADD:
        return "+";
    case OP_SUB:
        return "-";
    case OP_MUL:
        return "*";
    case OP_DIV:
        return "/";
    case OP_ABS:
        return "abs";
    case OP_NEG:
        return "neg";
    case OP_INC:
        return "add1";
    case OP_DEC:
        return "sub1";
    case OP_APP:
        return "string-append";
    case OP_PRT:
        return "print";
    case OP_CAR:
        return "car";
    case OP_CDR:
        return "cdr";
    case OP_BOX:
        return "box";
    case OP_UBX:
        return "unbox";
    case OP_CNS:
        return "cons";
    case CMP_GT:
        return ">";
    case CMP_GE:
        return ">=";
    case CMP_EQ:
        return "=";
    case CMP_LE:
        return "<=";
    case CMP_LT:
        return "<";
    case CMP_NE:
        return "~";
    case OP_FUN:
        return "fun";
    case OP_IDT:
        return "identity";
    case OP_AND:
        return "and";
    case OP_LOR:
        return "or";
    case CHKSTR:
        return "string?";
    case CHKLST:
        return "list?";
    case CHKELS:
        return "emptylist?";
    case CHKINT:
        return "integer?";
    case CHKFLG:
        return "flag?";
    case CHKBOO:
        return "boolean?";
    case CHKSYM:
        return "symbol?";
    case CHKPRC:
        return "proc?";
    case CHKBOX:
        return "box?";
    case CHKBYT:
        return "bytes?";
    case OP_LEN:
        return "length";
    case CMP_SE:
        return "symbol=?";
    case CMP_RE:
        return "string=?";
    case OP_MOD:
        return "mod";
    case BITAND:
        return "bitand";
    case BITLOR:
        return "bitor";
    case BITXOR:
        return "bitxor";
    case BITSHL:
        return "bitshl";
    case BITSHR:
        return "bitshr";
    case OP_NOT:
        return "not";
    case CHKCNS:
        return "cons?";
    case CMP_ZR:
        return "zero?";
    default:
        return "<undef-op>";
    }
}