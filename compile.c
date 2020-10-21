#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "lex.h"
#include "parse.h"
#include "compile.h"
#include "var.h"
#include <assert.h>
#include <unistd.h>

/* set to 1 when in tail position. */
static volatile int __tail__ = 0;
/* used to specify a no-write to stack */
static volatile int __nostacker__ = 0;
/* used to specify ignoring initializing memory to zero */
static volatile int __ignorezero__ = 0;

static volatile int __comp_trace__ = 0;
static volatile int __comp_asm_trace__ = 0;
static volatile int __trace_bind__ = 0;
static volatile int __comp_dbggc__ = 0;

void print_node_ (node_t* ast) {
    switch (ast->ntype) {
    case NODE_INTEG:
        printf("%li", ast->ival);
        break;
    case NODE_IBNOP:
        printf("(+ ");
        print_node_(ast->left);
        printf(" ");
        print_node_(ast->right);
        printf(")");
        break;
    }
}

void fprintf_c(FILE* dst, char* msg, ...) {
    if (__comp_asm_trace__) {
        va_list scrn;
        va_start(scrn, msg);
        vprintf(msg, scrn);
        va_end(scrn);
    }
    va_list fl;
    va_start(fl, msg);
    vfprintf(dst, msg, fl);
    va_end(fl);
}

int64_t mask_integer (int64_t i);
uint64_t mask_bool (uint8_t i);
void assert_integer (FILE *dst);
void assert_bool (FILE *dst);
void assert_type (FILE *dst, int bits, int tag);
void compile_expr (node_t* ast, FILE* dst, lexicon_t* table);
void compile_unary (node_t* ast, FILE* dst, lexicon_t* table);
void compile_binary (node_t* ast, FILE* dst, lexicon_t* table);
void compile_compare (node_t* ast, FILE* dst, lexicon_t* table, char* ins, int eq, int offset);
void compile_if (node_t* ast, FILE* dst, lexicon_t* table);
void compile_cond (node_t* ast, FILE* dst, int* off, char* el, char* af, lexicon_t* table);
void compile_letexp (node_t* ast, FILE* dst, lexicon_t* table);
char* gensym (char* buf, char* pre);
void buf_grow (char** buf, int* len, int* cap, int sz);
void compile_error (int l, int c, char* msg, ...);
void compile_call (node_t* ast, FILE* dst, lexicon_t* table, int proc);
void compile_define (node_t* ast, FILE* dst, lexicon_t* table);


void compile_closure (node_t* ast, FILE* dst, lexicon_t* table);
void lsapp (nodelist_t* ls, node_t* app);
void lambdas (node_t* ast, FILE* dst, lexicon_t* table, nodelist_t* ls);
void lfree (node_t* ast, FILE* dst, lexicon_t* table, scope_t* scope);


void compile_closure_create (node_t* ast, FILE* dst, lexicon_t* table);
void compile_closure_envcopy (FILE *dst);
void compile_letrec (node_t* ast, FILE* dst, lexicon_t* table);
void compile_closure_call (node_t* ast, FILE* dst, lexicon_t* table);
void compile_closure_tail_call (node_t* ast, FILE* dst, lexicon_t* table);
int bind__(lexicon_t* table, FILE* dst, int l, int c, char* val, int addoff);
int bind_(lexicon_t* table, FILE* dst, int l, int c, char* val);

int bind_(lexicon_t* table, FILE* dst, int l, int c, char* val) {
    bind__(table, dst, l, c, val, 0);
}

int bind__(lexicon_t* table, FILE* dst, int l, int c, char* val, int addoff) {
    int offset = bind(table, l, c, val);
    offset += addoff;
    
    if (__trace_bind__) printf("bound %s to offset %d, framesize %d\n", val, 8*offset, 8*lexicon_offset(table)-8);

    //fprintf_c(dst, "\tmov %s, [%s]\n", R11, "__gcstack__");
    fprintf_c(dst, "\tmov %s, [%s]\n", R13, "__gcframe__");
    //fprintf_c(dst, "\tsub %s, %s\n", R11, R13);
    fprintf_c(dst, "\tadd %s, %d\n", R13, COMPILER_WORDSIZE);
    fprintf_c(dst, "\tmov [%s], %s\n", "__gcframe__", R13);
    if (__nostacker__) {
        if (!__ignorezero__) {
            //fprintf_c(dst, "\tmov %s, %d\n", R11, 0);
            //fprintf_c(dst, "\tmov qword [%s], %d\n", R13, 0);
        }
        //fprintf_c(dst, "\tmov %s, 0\n", R13);
        //fprintf_c(dst, "\tmov [%s], %s\n", R11, R13);
    }
    else {
        //fprintf_c(dst, "\tmov %s, %s\n", R13, RSP);
        //fprintf_c(dst, "\tsub %s, %d\n", R13, offset * COMPILER_WORDSIZE);
        //fprintf_c(dst, "\tmov [%s], %s\n", R11, R13);
        /* ensure stack is zero-ed if unused right now */
        if (!__ignorezero__) {
            fprintf_c(dst, "\tmov %s, %d\n", R11, 0);
            fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * offset * COMPILER_WORDSIZE, R11);
        }
    }
    return offset;
}

void pop_scope_(lexicon_t* table, FILE* dst) {
    scope_t scope = table->scopes[table->len - 1];
    //int cnt = 0;
    for (int i = scope.len - 1; i >= 0; i--) {
        if (__trace_bind__) printf("unbound %s from offset %d, new size %d\n", scope.vars[i].name, 8*scope.vars[i].off, 8*(lexicon_offset(table)-((scope.len - i))));
        //cnt++;
    }
    int offset = pop_scope(table);
    if (offset == 0) return;
    fprintf_c(dst, "\tmov %s, [%s]\n", R13, "__gcframe__");
    fprintf_c(dst, "\tsub %s, %d\n", R13, offset * COMPILER_WORDSIZE);
    fprintf_c(dst, "\tmov [%s], %s\n", "__gcframe__", R13);
} 

char* nodespecies(node_t* ast);

static volatile uint32_t __gensym = 0;

extern volatile uint64_t __gcal__;
lexicon_t* globals;

void compile (char* path) {
    tokstring_t toks;
    toks.cap = 0;
    toks.len = 0;
    toks.off = 0;
    toks.toks = NULL;

    lexer_t lexer;
    
    /* read data from file */
    FILE* src = fopen(path, "r");
    int len = 0;
    int cap = 0;
    char* buf = NULL;
    buf_grow(&buf, &len, &cap, 4096);
    char ch = 0;
    while (ch != EOF) {
        ch = fgetc(src);
        if (2 * len >= cap) {
            buf_grow(&buf, &len, &cap, 0);
        }
        if (ch != EOF) {
            buf[len++] = ch;
            //printf("%c", ch);
        }
    }
    ch = '\n';
    for (int i = 0; i < 2; i++) {
        if (2 * len >= cap) {
            buf_grow(&buf, &len, &cap, 0);
        }
        buf[len++] = ch;
    }
    fclose(src);

    //printf("file read.\n");

    lex(&lexer, (uint8_t*) buf, strlen(buf), &toks);

    //printf("file lexed.\n");
    
    node_t* ast = parse(&toks);

    FILE* dst = fopen("out.s", "w+");

    fprintf_c(dst, "\tglobal entry\n\textern __frame__\n\textern __gc__\n\textern __gcstack__\n\textern __gcframe__\n\textern bytes_equal\n\textern __gcal__\n\textern error\n\textern gcalloc\n\textern print_result_\n\tglobal __globalsize__\n\tglobal __globalptr__\n\textern file_open_\n\textern file_write_\n\textern file_close_\n\textern symbol_append\n\tsection .data\n\talign 16\n");

    lexicon_t table;
    new_lexicon(&table);

    lexicon_t globals_;
    globals = &globals_;
    new_lexicon(globals);
    push_scope(globals);

    /* build dummies */
    for (int i = 0; i < ast->clauses->len; i++) {
        if (ast->clauses->clauses[i].ntype == NODE_DEFFN) {
            char* lbl_ = add_definition(&table, ast->clauses->clauses[i].sval, gensym, 1);
            fprintf_c(dst, "dummy%s dq 0, 0\n", lbl_);
        }
    }
    /* now we need to build globals */
    fprintf_c(dst, "align 16\n");
    int nglobes = 0;
    for (int i = 0; i < ast->clauses->len; i++) {
        if (ast->clauses->clauses[i].ntype == NODE_GLOBL) {
            char* lbl_ = add_definition(globals, ast->clauses->clauses[i].sval, gensym, 1);
            fprintf_c(dst, "glbl%s dq 0, 0\n", lbl_);
            nglobes++;
        }
    }
    fprintf_c(dst, "__globalsize__ dq %d\n", nglobes * 8);
    fprintf_c(dst, "__globalptr__ dq 0\n");

    fprintf_c(dst, "\tsection .text\n");
    
    /* build lexicon (scoper) */
    

    nodelist_t lms;
    nodelist_t* ls = &lms;
    ls->len = 0;
    ls->cap = 0;
    ls->ls = NULL;

    /* build the megalambda list */
    for (int i = 0; i < ast->clauses->len; i++) {
        lambdas(&(ast->clauses->clauses[i]), dst, &table, ls);
    }

    if (__comp_trace__) { printf("lambdasdone\n"); }

    /* compile closures */
    for (int i = 0; i < ls->len; i++) {
        compile_closure(ls->ls[i], dst, &table);
        if (__comp_trace__) { printf("SHOULD BE ZERO: %d\n", lexicon_offset(&table)); }
        if (__comp_trace__) { printf("SHOULD BE ZERO: %d\n", table.len); }
    }

    if (__comp_trace__) { printf("closuresdone\n"); }

    /* build definitions */
    for (int i = 0; i < ast->clauses->len; i++) {
        if (ast->clauses->clauses[i].ntype == NODE_DEFFN) {
            compile_define(&(ast->clauses->clauses[i]), dst, &table);
            if (__comp_trace__) { printf("SHOULD BE ZERO: %d\n", lexicon_offset(&table)); }
            if (__comp_trace__) { printf("SHOULD BE ZERO: %d\n", table.len); }
        }
    }

    if (__comp_trace__) { printf("definesdone\n"); }

    /* pop and repush globals so we don't have illegal global defs */
    pop_scope(globals);
    push_scope(globals);
    deflist_t* oldglbl = rem_deftable(globals);
    int __set_glblptr__ = 0;

    /* build programme */
    if (__comp_trace__) { printf("SHOULD BE ZERO: %d\n", lexicon_offset(&table)); }
    if (__comp_trace__) { printf("SHOULD BE ZERO: %d\n", table.len); }
    fprintf_c(dst, "\n\nalign 16\nentry:\n");
    /* utility code for global frame pointer */
    fprintf_c(dst, "\tmov qword [%s], %s\n", "__gcstack__", RSP);
    /* before main code, initialize dummies */
    for (int i = 0; i < ast->clauses->len; i++) {
        if (ast->clauses->clauses[i].ntype == NODE_DEFFN) {
            char* lbl = get_definition(&table, ast->clauses->clauses[i].sval);
            fprintf_c(dst, "\tlea %s, [%s]\n", RAX, lbl);
            fprintf_c(dst, "\tadd %s, %d\n", RAX, TAG_PROC);
            //fprintf_c(dst, "\tmov %s, 1\n", RBX);
            //fprintf_c(dst, "\tshl %s, 63\n", RBX);
            //fprintf_c(dst, "\tor %s, %s\n", RAX, RBX);
            fprintf_c(dst, "\tmov qword [%s%s], %s\n", "dummy", lbl, RAX);
            fprintf_c(dst, "\tmov %s, %d\n", RAX, TAG_ELIST);
            fprintf_c(dst, "\tmov qword [%s%s + %d], %s\n", "dummy", lbl, COMPILER_WORDSIZE, RAX);
        }
        else if (ast->clauses->clauses[i].ntype == NODE_GLOBL) {
            compile_expr(ast->clauses->clauses[i].left, dst, &table);
            reintroduce_def(globals, oldglbl, ast->clauses->clauses[i].sval);
            fprintf_c(dst, "\tmov [glbl%s], %s\n", get_definition(globals, ast->clauses->clauses[i].sval), RAX);
            if (!__set_glblptr__) {
                fprintf_c(dst, "\tlea %s, [glbl%s]\n", RAX, get_definition(globals, ast->clauses->clauses[i].sval));
                fprintf_c(dst, "\tmov [%s], %s\n", "__globalptr__", RAX);
                __set_glblptr__ = 1;
            }
        }
    }
    free(oldglbl);
    for (int i = 0; i < ast->clauses->len; i++) {
        if (ast->clauses->clauses[i].ntype != NODE_DEFFN && ast->clauses->clauses[i].ntype != NODE_GLOBL) {
            compile_expr(&(ast->clauses->clauses[i]), dst, &table);
            //fprintf_c(dst, "\tpush %s\n", RSP);
            if (!(ast->clauses->clauses[i].supress)) {
                fprintf_c(dst, "\tpush %s\n", RDI);
                fprintf_c(dst, "\tmov %s, %s\n", RDI, RAX);
                fprintf_c(dst, "\tcall print_result_\n");
                //fprintf_c(dst, "\tadd %s, 8\n", RSP);
                fprintf_c(dst, "\tpop %s\n", RDI);
                //fprintf_c(dst, "\tpop %s\n", RSP);
            }
        }
    }

    if (__comp_trace__) { printf("programdone\n"); }

    if (__comp_trace__) { printf("SHOULD BE ZERO: %d\n", lexicon_offset(&table)); }
    if (__comp_trace__) { printf("SHOULD BE ZERO: %d\n", table.len); }

    /* compile_expr(ast, dst, &table); */
    if (__comp_dbggc__) {
        fprintf_c(dst, "\tmov %s, [__gcal__]\n", RDI);
        fprintf_c(dst, "\tshl %s, 6\n", RDI);
        fprintf_c(dst, "\tcall print_result_\n");
    }
    fprintf_c(dst, "\tret\n\n");
    
    /* footer */
    fprintf_c(dst, "%s:\n", ERR);
    if (__comp_dbggc__) {
        fprintf_c(dst, "\tmov %s, [__gcal__]\n", RDI);
        fprintf_c(dst, "\tshl %s, 6\n", RDI);
        fprintf_c(dst, "\tcall print_result_\n");
    }
    fprintf_c(dst, "\tpush %s\n", RBP);
    fprintf_c(dst, "\tmov %s, %s\n", RDI, R8);
    fprintf_c(dst, "\tmov %s, %s\n", RSI, RAX);
    fprintf_c(dst, "\tcall error\n");

    fclose(dst);
}

int64_t mask_integer (int64_t i) {
    return ((* (uint64_t*) &i) << 6) | TAG_INT;
}

uint64_t mask_bool (uint8_t i) {
    return (((uint64_t) (i ? 1 : 0)) << 6) | TAG_FLAG;
}

void assert_integer (FILE* dst) {
    fprintf_c(dst, "\tmov %s, %s\n", RBX, RAX);
    fprintf_c(dst, "\tand %s, %d\n", RBX, TAG_BITS);
    fprintf_c(dst, "\tcmp %s, %d\n", RBX, TAG_INT);
    fprintf_c(dst, "\tmov %s, %d\n", R8, ERR_ASSERT_INTEGER);
    fprintf_c(dst, "\tjne %s\n", ERR);
}

void assert_bool (FILE *dst) {
    fprintf_c(dst, "\tmov %s, %s\n", RBX, RAX);
    fprintf_c(dst, "\tand %s, %d\n", RBX, TAG_BITS);
    fprintf_c(dst, "\tcmp %s, %d\n", RBX, TAG_FLAG);
    fprintf_c(dst, "\tmov %s, %d\n", R8, ERR_ASSERT_BOOLEAN);
    fprintf_c(dst, "\tjne %s\n", ERR);
}

void assert_type (FILE *dst, int bits, int tag) {
    fprintf_c(dst, "\tmov %s, %s\n", RBX, RAX);
    fprintf_c(dst, "\tand %s, %d\n", RBX, ((1 << bits) - 1));
    fprintf_c(dst, "\tcmp %s, %d\n", RBX, tag);
    if (tag > 7) {
        fprintf_c(dst, "\tmov %s, %d\n", R8, 2 * tag);
    }
    else {
        fprintf_c(dst, "\tmov %s, %d\n", R8, 64 * tag);
    }
    fprintf_c(dst, "\tjne %s\n", ERR);
}

void compile_expr (node_t* ast, FILE* dst, lexicon_t* table) {
    char sym1[30] = {0};
    char sym2[30] = {0};
    int offset;

    switch (ast->ntype) {
    case NODE_UQOPR:
        if (__comp_trace__) { printf("compile_quote_operation\n"); }
        {
            int __tail = __tail__;
            __tail__ = 0;
            char sym[30] = {0};
            gensym(&sym[0], "unquot");
            compile_expr(ast->left, dst, table);
            assert_type(dst, 3, TAG_SYMBOL);
            fprintf_c(dst, "\tmov %s, %s\n", RBX, RAX);
            fprintf_c(dst, "\tshr %s, %d\n", RBX, 8);
            fprintf_c(dst, "\tand %s, %d\n", RBX, 255);
            fprintf_c(dst, "\tcmp %s, %d\n", RBX, 255);
            fprintf_c(dst, "\tje %s\n", &sym[0]);
            fprintf_c(dst, "\tmov %s, %s\n", R10, RAX);
            fprintf_c(dst, "\tshr %s, %d\n", R10, 16);
            fprintf_c(dst, "\tmov %s, [%s]\n", RAX, R10);
            fprintf_c(dst, "%s:\n", &sym[0]);
            __tail__ = __tail;
        }
        break;
    case NODE_QTOPR:
        if (__comp_trace__) { printf("compile_quote_operation\n"); }
        {
            int __tail = __tail__;
            __tail__ = 0;
            char sym[30] = {0};
            char sy2[30] = {0};
            //char sy3[30] = {0};
            gensym(&sym[0], "quote");
            gensym(&sy2[0], "quote");
            //gensym(&sy3[0], "quote");
            push_scope(table);
            int __offset = bind_(table, dst, 0, 0, "****");
            compile_expr(ast->left, dst, table);
            fprintf_c(dst, "\tmov %s, %s\n", RCX, RAX);
            fprintf_c(dst, "\tand %s, %d\n", RCX, 7);
            fprintf_c(dst, "\tcmp %s, %d\n", RCX, 5);
            fprintf_c(dst, "\tje %s\n", &sy2[0]);
            fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * __offset, RAX);
            //fprintf_c(dst, "\tmov %s, %s\n", R15, RSP);
            //fprintf_c(dst, "\tmov %s, %d\n", R14, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            fprintf_c(dst, "\tsub %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            fprintf_c(dst, "\tcall gcalloc\n");
            fprintf_c(dst, "\tadd %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            fprintf_c(dst, "\tmov %s, [%s + %d]\n", RAX, RSP, -1 * COMPILER_WORDSIZE * __offset);
            fprintf_c(dst, "\tmov [%s], %s\n", RDI, RAX);
            fprintf_c(dst, "\tmov %s, %s\n", RCX, RAX);
            fprintf_c(dst, "\tmov %s, %s\n", RBX, RAX);
            fprintf_c(dst, "\tand %s, %d\n", RBX, TAG_BITS);
            fprintf_c(dst, "\tand %s, %d\n", RCX, 7);
            fprintf_c(dst, "\tcmp %s, %d\n", RCX, 0);
            fprintf_c(dst, "\tmov %s, %s\n", RCX, RBX);
            fprintf_c(dst, "\tje %s\n", &sym[0]);
            fprintf_c(dst, "\tand %s, %d\n", RCX, 7);
            fprintf_c(dst, "%s:\n", &sym[0]);
            fprintf_c(dst, "\tmov %s, %s\n", RAX, RDI);
            fprintf_c(dst, "\tshl %s, %d\n", RAX, 8);
            fprintf_c(dst, "\tor %s, %s\n", RAX, RCX);
            fprintf_c(dst, "\tshl %s, %d\n", RAX, 8);
            fprintf_c(dst, "\tor %s, %d\n", RAX, TAG_SYMBOL);
            fprintf_c(dst, "%s:\n", &sy2[0]);
            pop_scope_(table, dst);
            __tail__ = __tail;
        }
        break;
    case NODE_INTEG:
        if (__comp_trace__) { printf("compile_integer %li\n", ast->ival); }
        if (((ast->ival < 0) && ((-1 * ast->ival) >> 58)) \
          && ((ast->ival > 0) && (ast->ival >> 58))) {
            compile_error(ast->l, ast->c, "integer too big: %d", ast->ival);
        }
        fprintf_c(dst, "\tmov %s, %li\n", RAX, mask_integer(ast->ival));
        if (__comp_trace__) { printf("DONE compile_integer %li\n", ast->ival); }
        break;
    case NODE_BOOLV:
        if (__comp_trace__) { printf("compile_bool %s\n", ast->bval ? "#t" : "#f"); }
        fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(ast->bval));
        if (__comp_trace__) { printf("DONE compile_integer %s\n", ast->bval ? "#t" : "#f"); }
        break;
    case NODE_STRNG:
        /* pointer to linked list of string data */
        if (__comp_trace__) { printf("compile_string %s\n", ast->sval); }
        {
            push_scope(table);
            int __slen = strlen(ast->sval);
            int __sval = __slen / 7;
            int __offset = bind_(table, dst, 0, 0, "*");
            fprintf_c(dst, "\tmov %s, %d\n", R10, TAG_ELIST);
            //fprintf_c(dst, "\tmov %s, %s\n", R15, RSP);
            //fprintf_c(dst, "\tmov %s, %d\n", R14, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            for (int i = 0; i <= __sval; i++ ) {
                if (i * 7 >= __slen) break;
                fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * __offset, R10);
                fprintf_c(dst, "\tsub %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
                fprintf_c(dst, "\tcall gcalloc\n");
                fprintf_c(dst, "\tadd %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
                fprintf_c(dst, "\tmov %s, [%s + %d]\n", R10, RSP, -1 * COMPILER_WORDSIZE * __offset);
                fprintf_c(dst, "\txor %s, %s\n", RAX, RAX);
                for (int j = 0; j < ((i < __sval) ? 7 : (__slen % 7)); j++) {
                    fprintf_c(dst, "\tor %s, %d\n", RAX, ast->sval[__slen - (7*i + j + 1)]);
                    if ((i*7 + j == __slen - 1)  && (__slen % 7)) {
                        fprintf_c(dst, "\tshl %s, %d\n", RAX, 8 * (8 - (__slen % 7)));
                    } else {
                        fprintf_c(dst, "\tshl %s, 8\n", RAX);
                    }
                }
                fprintf_c(dst, "\tor %s, %d\n", RAX, TAG_BYTES);
                fprintf_c(dst, "\tmov [%s], %s\n", RDI, RAX);
                fprintf_c(dst, "\tmov [%s + %d], %s\n", RDI, COMPILER_WORDSIZE, R10);
                fprintf_c(dst, "\tmov %s, %s\n", R10, RDI);
                fprintf_c(dst, "\tor %s, %d\n", R10, TAG_LIST);
            }
            fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * __offset, R10);
            fprintf_c(dst, "\tsub %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            fprintf_c(dst, "\tcall gcalloc\n");
            fprintf_c(dst, "\tadd %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            fprintf_c(dst, "\tmov %s, [%s + %d]\n", R10, RSP, -1 * COMPILER_WORDSIZE * __offset);
            fprintf_c(dst, "\tmov [%s], %s\n", RDI, R10);
            fprintf_c(dst, "\tmov %s, %s\n", RAX, RDI);
            fprintf_c(dst, "\tor %s, %d\n", RAX, TAG_STRING);
            pop_scope_(table, dst);
        }
        break;
    case NODE_LQUOT:
        /* pointer to linked list of symbol data */
        if (__comp_trace__) { printf("compile_primitive_symbol %s\n", ast->sval); }
        {
            push_scope(table);
            int __slen = strlen(ast->sval);
            int __sval = __slen / 7;
            int __offset = bind_(table, dst, 0, 0, "*");
            fprintf_c(dst, "\tmov %s, %d\n", R10, TAG_ELIST);
            //fprintf_c(dst, "\tmov %s, %s\n", R15, RSP);
            //fprintf_c(dst, "\tmov %s, %d\n", R14, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            for (int i = 0; i <= __sval; i++ ) {
                if (i * 7 >= __slen) break;
                fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * __offset, R10);
                fprintf_c(dst, "\tsub %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
                fprintf_c(dst, "\tcall gcalloc\n");
                fprintf_c(dst, "\tadd %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
                fprintf_c(dst, "\tmov %s, [%s + %d]\n", R10, RSP, -1 * COMPILER_WORDSIZE * __offset);
                fprintf_c(dst, "\txor %s, %s\n", RAX, RAX);
                for (int j = 0; j < ((i < __sval) ? 7 : (__slen % 7)); j++) {
                    fprintf_c(dst, "\tor %s, %d\n", RAX, ast->sval[__slen - (7*i + j + 1)]);
                    if ((i*7 + j == __slen - 1) && (__slen % 7)) {
                        fprintf_c(dst, "\tshl %s, %d\n", RAX, 8 * (8 - (__slen % 7)));
                    } else {
                        fprintf_c(dst, "\tshl %s, 8\n", RAX);
                    }
                }
                fprintf_c(dst, "\tor %s, %d\n", RAX, TAG_BYTES);
                fprintf_c(dst, "\tmov [%s], %s\n", RDI, RAX);
                fprintf_c(dst, "\tmov [%s + %d], %s\n", RDI, COMPILER_WORDSIZE, R10);
                fprintf_c(dst, "\tmov %s, %s\n", R10, RDI);
                fprintf_c(dst, "\tor %s, %d\n", R10, TAG_LIST);
            }
            fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * __offset, R10);
            fprintf_c(dst, "\tsub %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            fprintf_c(dst, "\tcall gcalloc\n");
            fprintf_c(dst, "\tadd %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            fprintf_c(dst, "\tmov %s, [%s + %d]\n", R10, RSP, -1 * COMPILER_WORDSIZE * __offset);
            fprintf_c(dst, "\tmov [%s], %s\n", RDI, R10);
            fprintf_c(dst, "\tmov %s, %s\n", RAX, RDI);
            fprintf_c(dst, "\tshl %s, %d\n", RAX, 8);
            fprintf_c(dst, "\tor %s, %d\n", RAX, TAG_SYMBOL_DEF);
            fprintf_c(dst, "\tshl %s, %d\n", RAX, 8);
            fprintf_c(dst, "\tor %s, %d\n", RAX, TAG_SYMBOL);
            pop_scope_(table, dst);
        }
        break;
    case NODE_LQFLG:
        //fprintf_c(dst, "\tmov %s, %s\n", R15, RSP);
        //fprintf_c(dst, "\tmov %s, %d\n", R14, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
        fprintf_c(dst, "\tsub %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
        fprintf_c(dst, "\tcall gcalloc\n");
        fprintf_c(dst, "\tadd %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
        fprintf_c(dst, "\tmov %s, %d\n", RAX, mask_bool(ast->bval));
        fprintf_c(dst, "\tmov [%s], %s\n", RDI, RAX);
        fprintf_c(dst, "\tmov %s, %s\n", RAX, RDI);
        fprintf_c(dst, "\tshl %s, %d\n", RAX, 8);
        fprintf_c(dst, "\tor %s, %d\n", RAX, TAG_FLAG);
        fprintf_c(dst, "\tshl %s, %d\n", RAX, 8);
        fprintf_c(dst, "\tor %s, %d\n", RAX, TAG_SYMBOL);
        break;
    case NODE_LQINT:
        //fprintf_c(dst, "\tmov %s, %s\n", R15, RSP);
        //fprintf_c(dst, "\tmov %s, %d\n", R14, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
        fprintf_c(dst, "\tsub %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
        fprintf_c(dst, "\tcall gcalloc\n");
        fprintf_c(dst, "\tadd %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
        fprintf_c(dst, "\tmov %s, %d\n", RAX, mask_integer(ast->ival));
        fprintf_c(dst, "\tmov [%s], %s\n", RDI, RAX);
        fprintf_c(dst, "\tmov %s, %s\n", RAX, RDI);
        fprintf_c(dst, "\tshl %s, %d\n", RAX, 8);
        fprintf_c(dst, "\tor %s, %d\n", RAX, TAG_INT);
        fprintf_c(dst, "\tshl %s, %d\n", RAX, 8);
        fprintf_c(dst, "\tor %s, %d\n", RAX, TAG_SYMBOL);
        break;
    case NODE_LQSTR:
        {
            /* build string */
            node_t ast2;
            newnode(&ast2);
            ast2.sval = ast->sval;
            ast2.ntype = NODE_STRNG;
            push_scope(table);
            int __offset = bind_(table, dst, 0, 0, "***");
            compile_expr(&ast2, dst, table);

            fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * __offset, RAX);
            //fprintf_c(dst, "\tmov %s, %s\n", R15, RSP);
            //fprintf_c(dst, "\tmov %s, %d\n", R14, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            fprintf_c(dst, "\tsub %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            fprintf_c(dst, "\tcall gcalloc\n");
            fprintf_c(dst, "\tadd %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            fprintf_c(dst, "\tmov %s, [%s + %d]\n", RAX, RSP, -1 * COMPILER_WORDSIZE * __offset);
            fprintf_c(dst, "\tmov [%s], %s\n", RDI, RAX);
            fprintf_c(dst, "\tmov %s, %s\n", RAX, RDI);
            fprintf_c(dst, "\tshl %s, %d\n", RAX, 8);
            fprintf_c(dst, "\tor %s, %d\n", RAX, TAG_STRING);
            fprintf_c(dst, "\tshl %s, %d\n", RAX, 8);
            fprintf_c(dst, "\tor %s, %d\n", RAX, TAG_SYMBOL);
            pop_scope_(table, dst);
        }
        break;
    case NODE_CQUOT:
        /* basically a quoted list */
        /* a list of things to cons together. not in ANF. */
        if (ast->clauses->len == 1) {
            /* then it's '() */
            //fprintf_c(dst, "\tmov %s, %s\n", R15, RSP);
            //fprintf_c(dst, "\tmov %s, %d\n", R14, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            fprintf_c(dst, "\tsub %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            fprintf_c(dst, "\tcall gcalloc\n");
            fprintf_c(dst, "\tadd %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            fprintf_c(dst, "\tmov %s, %d\n", RAX, TAG_ELIST);
            fprintf_c(dst, "\tmov [%s], %s\n", RDI, RAX);
            fprintf_c(dst, "\tmov %s, %s\n", RAX, RDI);
            fprintf_c(dst, "\tshl %s, %d\n", RAX, 8);
            fprintf_c(dst, "\tor %s, %d\n", RAX, TAG_ELIST);
            fprintf_c(dst, "\tshl %s, %d\n", RAX, 8);
            fprintf_c(dst, "\tor %s, %d\n", RAX, TAG_SYMBOL);
            break;
        }
        push_scope(table);
        int r10offset = bind_(table, dst, 0, 0, "**");
        push_scope(table);
        int offset = bind_(table, dst, 0, 0, "*");
        int offs3t = bind_(table, dst, 0, 0, "***");
        for (int i = ast->clauses->len - 2; i >= 0; i--) {
            int __tail = __tail__;
            __tail__ = 0;
            if (i < ast->clauses->len - 2) {
                fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * r10offset, RAX);
            }
            
            if (__comp_trace__) { printf("compile_cons\n"); }
            compile_expr(&(ast->clauses->clauses[i]), dst, table);
            fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * offset, RAX);
            if (i == ast->clauses->len - 2) {
                compile_expr(&(ast->clauses->clauses[i + 1]), dst, table);
            } else {
                fprintf_c(dst, "\tmov %s, [%s + %d]\n", RAX, RSP, -1 * COMPILER_WORDSIZE * r10offset);
            }
            //fprintf_c(dst, "\tmov %s, %s\n", R15, RSP);
            //fprintf_c(dst, "\tmov %s, %d\n", R14, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * offs3t, RAX);
            fprintf_c(dst, "\tsub %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            fprintf_c(dst, "\tcall gcalloc\n");
            fprintf_c(dst, "\tadd %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            fprintf_c(dst, "\tmov %s, [%s + %d]\n", RAX, RSP, -1 * COMPILER_WORDSIZE * offs3t);
            fprintf_c(dst, "\tmov %s, [%s + %d]\n", RCX, RSP, -1 * COMPILER_WORDSIZE * offset);
            fprintf_c(dst, "\tmov [%s], %s\n", RDI, RCX);
            fprintf_c(dst, "\tmov [%s + %d], %s\n", RDI, COMPILER_WORDSIZE, RAX);
            fprintf_c(dst, "\tmov %s, %s\n", RAX, RDI);
            fprintf_c(dst, "\tor %s, %d\n", RAX, TAG_LIST);
            if (__comp_trace__) { printf("DONE compile_cons\n"); }
            __tail__ = __tail;
        }
        pop_scope_(table, dst);
        fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * r10offset, RAX);
        fprintf_c(dst, "\tsub %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
        fprintf_c(dst, "\tcall gcalloc\n");
        fprintf_c(dst, "\tadd %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
        fprintf_c(dst, "\tmov %s, [%s + %d]\n", RAX, RSP, -1 * COMPILER_WORDSIZE * r10offset);
        fprintf_c(dst, "\tmov [%s], %s\n", RDI, RAX);
        fprintf_c(dst, "\tmov %s, %s\n", RAX, RDI);
        fprintf_c(dst, "\tshl %s, %d\n", RAX, 8);
        fprintf_c(dst, "\tor %s, %d\n", RAX, TAG_LIST);
        fprintf_c(dst, "\tshl %s, %d\n", RAX, 8);
        fprintf_c(dst, "\tor %s, %d\n", RAX, TAG_SYMBOL);
        pop_scope_(table, dst);
        break;
    case NODE_VARBL:
        offset = lookup(table, ast->sval);
        if (offset == LEXICON_NOTFOUND) {
            if (get_args(table, ast->sval) == -1) {
                if (__comp_trace__) { printf("compile_global %s\n", ast->sval); }
                if (get_definition(globals, ast->sval) == NULL) {
                    print_table(table);
                    compile_error(ast->l, ast->c, "cannot compile variable or fun or global of undefined or non-definition \'%s\'\n", ast->sval);
                }
                fprintf_c(dst, "\tmov %s, [glbl%s]\n", RAX, get_definition(globals, ast->sval));
                break;
            }
            if (__comp_trace__) { printf("compile_function %s\n", ast->sval); }
            push_scope(table);
            int __offset = bind_(table, dst, 0, 0, "********");
            fprintf_c(dst, "\tlea %s, [%s]\n", RAX, get_definition(table, ast->sval));
            fprintf_c(dst, "\tadd %s, %d\n", RAX, TAG_PROC);
            //fprintf_c(dst, "\tmov %s, 1\n", RBX);
            //fprintf_c(dst, "\tshl %s, 63\n", RBX);
            //fprintf_c(dst, "\tor %s, %s\n", RAX, RBX);
            fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * __offset, RAX);
            //fprintf_c(dst, "\tmov %s, %s\n", R15, RSP);
            //fprintf_c(dst, "\tmov %s, %d\n", R14, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            fprintf_c(dst, "\tsub %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            fprintf_c(dst, "\tcall gcalloc\n");
            fprintf_c(dst, "\tadd %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            fprintf_c(dst, "\tmov %s, [%s + %d]\n", RAX, RSP, -1 * COMPILER_WORDSIZE * __offset);
            fprintf_c(dst, "\tmov [%s], %s\n", RDI, RAX);
            fprintf_c(dst, "\tmov %s, %d\n", RAX, TAG_ELIST);
            fprintf_c(dst, "\tmov [%s + %d], %s\n", RDI, COMPILER_WORDSIZE, RAX);
            fprintf_c(dst, "\tmov %s, %s\n", RAX, RDI);
            fprintf_c(dst, "\tor %s, %d\n", RAX, TAG_PROC);
            pop_scope_(table, dst);
            if (__comp_trace__) { printf("DONE compile_function %s\n", ast->sval); }
            break;
        }
        if (__comp_trace__) { printf("compile_ident %s\n", ast->sval); }
        fprintf_c(dst, "\tmov %s, [%s + %d]\n", RAX, RSP, -1 * COMPILER_WORDSIZE * offset);
        if (__comp_trace__) { printf("DONE compile_idenT %s\n", ast->sval); }
        break;
    case NODE_ELIST:
        if (__comp_trace__) { printf("compile_emptylist\n"); }
        fprintf_c(dst, "\tmov %s, %d\n", RAX, TAG_ELIST);
        if (__comp_trace__) { printf("DONE compile_emptylist\n"); }
        break;
    case NODE_IUNOP:
        compile_unary(ast, dst, table);
        break;
    case NODE_IFEXP:
        compile_if(ast, dst, table);
        break;
    case NODE_CONDE:
        gensym(&sym1[0], "else");
        gensym(&sym2[0], "else");
        int off = 0;
        if (__comp_trace__) { printf("compile_cond\n"); }
        compile_cond(ast, dst, &off, &sym1[0], &sym2[0], table);
        if (__comp_trace__) { printf("DONE compile_cond\n"); }
        break;
    case NODE_IBNOP:
        //print_node_(ast);
        //printf("\n");
        compile_binary(ast, dst, table);
        break;
    case NODE_LETEX:
        /* calls helper */
        compile_letexp(ast, dst, table);
        break;
    case NODE_CALLF:
        compile_call(ast, dst, table, 0);
        break;
    case NODE_FLOPN:
        {
            int __tail = __tail__;
            __tail__ = 0;
            compile_expr(ast->left, dst, table);
            assert_type(dst, 3, TAG_STRING);
            fprintf_c(dst, "\tmov %s, %s\n", RDI, RAX);
            fprintf_c(dst, "\tsub %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            fprintf_c(dst, "\tcall file_open_\n");
            fprintf_c(dst, "\tadd %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            __tail__ = __tail;
        }
        break;
    case NODE_FLWRT:
        {
            int __tail = __tail__;
            __tail__ = 0;
            compile_expr(ast->left, dst, table);
            fprintf_c(dst, "\tmov %s, %s\n", RDI, RAX);
            fprintf_c(dst, "\tsub %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            fprintf_c(dst, "\tcall file_write_\n");
            fprintf_c(dst, "\tadd %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            __tail__ = __tail;
        }
        break;
    case NODE_FLCLS:
        {
            int __tail = __tail__;
            __tail__ = 0;
            fprintf_c(dst, "\tsub %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            fprintf_c(dst, "\tcall file_close_\n");
            fprintf_c(dst, "\tadd %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            __tail__ = __tail;
        }
        break;
    case NODE_MULTI:
        {
            if (__comp_trace__) printf("multi\n");
            int __tail = __tail__;
            __tail__ = 0;
            compile_expr(ast->left, dst, table);
            fprintf_c(dst, "\txor %s, %s\n", RAX, RAX);
            __tail__ = __tail;
            /* this can be safely tail called */
            compile_expr(ast->right, dst, table);
        }
        break;
    case NODE_MUTST:
        {
            int __tail = __tail__;
            __tail__ = 0;
            offset = lookup(table, ast->sval);
            if (offset == LEXICON_NOTFOUND) {
                if (get_definition(table, ast->sval) != NULL) {
                    compile_error(ast->l, ast->c, "cannot mutate immutable definition \'%s\'\n", ast->sval);
                }
                if (__comp_trace__) { printf("compile_global %s\n", ast->sval); }
                if (get_definition(globals, ast->sval) == NULL) {
                    compile_error(ast->l, ast->c, "cannot mutate undefined lexical code point \'%s\'\n", ast->sval);
                }
                compile_expr(ast->left, dst, table);
                fprintf_c(dst, "\tmov [glbl%s], %s\n", get_definition(globals, ast->sval), RAX);
                break;
            }
            if (__comp_trace__) { printf("compile_ident %s\n", ast->sval); }
            compile_expr(ast->left, dst, table);
            fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * offset, RAX);
            if (__comp_trace__) { printf("DONE compile_ident %s\n", ast->sval); }
            __tail__ = __tail;
            break;
        }
        break;
    case NODE_CLOSE:
        compile_closure_create(ast, dst, table);
        break;
    default:
        compile_error(ast->l, ast->c, "fatal error occured.");
    }
}

void compile_letexp (node_t* ast, FILE* dst, lexicon_t* table) {
    if (__comp_trace__) { printf("compile_let\n"); }
    /* we make a new scope */
    push_scope(table);

    int offset;
    int _tail = __tail__;
    __tail__ = 0;
    /* iterate over everything to build bindings */
    for (int i = 0; i < ast->clauses->len; i++) {
        //printf("let: %s\n", ast->clauses->clauses[i].sval);
        offset = bind_(table, dst, ast->clauses->clauses[i].l, ast->clauses->clauses[i].c, ast->clauses->clauses[i].sval);
        compile_expr(ast->clauses->clauses[i].left, dst, table);
        fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * offset, RAX);
    }
    __tail__ = _tail;

    /* build expression now */
    compile_expr(ast->left, dst, table);

    /* at the end we pop the scope */
    pop_scope_(table, dst);
    if (__comp_trace__) { printf("DONE compile_let\n"); }
}

void compile_binary (node_t* ast, FILE* dst, lexicon_t* table) {
    int _tail = __tail__;
    __tail__ = 0;
    char sym[30] = {0}; /* time for gensym to SHINE, muthatruckas. */
    gensym(&sym[0], "binary");
    push_scope(table);
    int offset = bind_(table, dst, ast->l, ast->c, &sym[0]);
    switch (ast->otype) {
    case OP_SAP:
        compile_expr(ast->left, dst, table);
        assert_type(dst, 16, (255 << 8) | TAG_SYMBOL);
        fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * offset, RAX);
        compile_expr(ast->right, dst, table);
        assert_type(dst, 16, (255 << 8) | TAG_SYMBOL);
        fprintf_c(dst, "\tmov %s, %s\n", RSI, RAX);
        fprintf_c(dst, "\tmov %s, [%s + %d]\n", RDI, RSP, -1 * COMPILER_WORDSIZE * offset);
        fprintf_c(dst, "\tsub %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
        fprintf_c(dst, "\tcall symbol_append\n");
        fprintf_c(dst, "\tadd %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
        break;
    case OP_ADD:
        if (__comp_trace__) { printf("compile_add\n"); }
        compile_expr(ast->left, dst, table);
        assert_integer(dst);
        fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * offset, RAX);
        compile_expr(ast->right, dst, table);
        assert_integer(dst);
        fprintf_c(dst, "\tadd %s, [%s + %d]\n", RAX, RSP, -1 * COMPILER_WORDSIZE * offset);
        if (__comp_trace__) { printf("DONE compile_add\n"); }
        break;
    case OP_SUB:
        if (__comp_trace__) { printf("compile_sub\n"); }
        compile_expr(ast->left, dst, table);
        assert_integer(dst);
        fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * offset, RAX);
        compile_expr(ast->right, dst, table);
        assert_integer(dst);
        fprintf_c(dst, "\tsub %s, [%s + %d]\n", RAX, RSP, -1 * COMPILER_WORDSIZE * offset);
        if (__comp_trace__) { printf("DONE compile_sub\n"); }
        break;
    case OP_MUL:
        if (__comp_trace__) { printf("compile_mul\n"); }
        compile_expr(ast->left, dst, table);
        assert_integer(dst);
        fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * offset, RAX);
        compile_expr(ast->right, dst, table);
        assert_integer(dst);
        fprintf_c(dst, "\tmov %s, %s\n", RBX, RAX);
        fprintf_c(dst, "\tmov %s, [%s + %d]\n", RAX, RSP, -1 * COMPILER_WORDSIZE * offset);
        fprintf_c(dst, "\tmul %s\n", RBX);
        fprintf_c(dst, "\tshr %s, 6\n", RAX);
        if (__comp_trace__) { printf("DONE compile_mul\n"); }
        break;
    case OP_MOD:
        if (__comp_trace__) { printf("compile_mod\n"); }
        compile_expr(ast->left, dst, table);
        assert_integer(dst);
        fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * offset, RAX);
        compile_expr(ast->right, dst, table);
        assert_integer(dst);
        fprintf_c(dst, "\tmov %s, %s\n", RBX, RAX);
        fprintf_c(dst, "\tmov %s, [%s + %d]\n", RAX, RSP, -1 * COMPILER_WORDSIZE * offset);
        fprintf_c(dst, "\txor %s, %s\n", RDX, RDX);
        fprintf_c(dst, "\tshr %s, 6\n", RAX);
        fprintf_c(dst, "\tshr %s, 6\n", RBX);
        fprintf_c(dst, "\tdiv %s\n", RBX);
        fprintf_c(dst, "\tmov %s, %s\n", RAX, RDX);
        fprintf_c(dst, "\tshl %s, 6\n", RAX);
        if (__comp_trace__) { printf("DONE compile_mod\n"); }
        break;
    case BITAND:
        if (__comp_trace__) { printf("compile_bitand\n"); }
        compile_expr(ast->left, dst, table);
        assert_integer(dst);
        fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * offset, RAX);
        compile_expr(ast->right, dst, table);
        assert_integer(dst);
        fprintf_c(dst, "\tand %s, [%s + %d]\n", RAX, RSP, -1 * COMPILER_WORDSIZE * offset);
        if (__comp_trace__) { printf("DONE compile_bitand\n"); }
        break;
    case BITLOR:
        if (__comp_trace__) { printf("compile_bitor\n"); }
        compile_expr(ast->left, dst, table);
        assert_integer(dst);
        fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * offset, RAX);
        compile_expr(ast->right, dst, table);
        assert_integer(dst);
        fprintf_c(dst, "\tor %s, [%s + %d]\n", RAX, RSP, -1 * COMPILER_WORDSIZE * offset);
        if (__comp_trace__) { printf("DONE compile_bitor\n"); }
        break;
    case BITXOR:
        if (__comp_trace__) { printf("compile_bitxor\n"); }
        compile_expr(ast->left, dst, table);
        assert_integer(dst);
        fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * offset, RAX);
        compile_expr(ast->right, dst, table);
        assert_integer(dst);
        fprintf_c(dst, "\txor %s, [%s + %d]\n", RAX, RSP, -1 * COMPILER_WORDSIZE * offset);
        if (__comp_trace__) { printf("DONE compile_bitxor\n"); }
        break;
    case BITSHL:
        if (__comp_trace__) { printf("compile_bitshl\n"); }
        compile_expr(ast->left, dst, table);
        assert_integer(dst);
        fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * offset, RAX);
        compile_expr(ast->right, dst, table);
        assert_integer(dst);
        fprintf_c(dst, "\tmov %s, [%s + %d]\n", RBX, RSP, -1 * COMPILER_WORDSIZE * offset);
        fprintf_c(dst, "\tshl %s, %s\n", RBX, RAX);
        fprintf_c(dst, "\tmov %s, %s\n", RAX, RBX);
        if (__comp_trace__) { printf("DONE compile_bitshl\n"); }
        break;
    case BITSHR:
        if (__comp_trace__) { printf("compile_bitshr\n"); }
        compile_expr(ast->left, dst, table);
        assert_integer(dst);
        fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * offset, RAX);
        compile_expr(ast->right, dst, table);
        assert_integer(dst);
        fprintf_c(dst, "\tmov %s, [%s + %d]\n", RBX, RSP, -1 * COMPILER_WORDSIZE * offset);
        fprintf_c(dst, "\tshr %s, %s\n", RBX, RAX);
        fprintf_c(dst, "\tmov %s, %s\n", RAX, RBX);
        if (__comp_trace__) { printf("DONE compile_bitshr\n"); }
        break;
    case OP_DIV:
        if (__comp_trace__) { printf("compile_div\n"); }
        compile_expr(ast->left, dst, table);
        assert_integer(dst);
        fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * offset, RAX);
        compile_expr(ast->right, dst, table);
        assert_integer(dst);
        fprintf_c(dst, "\tmov %s, %s\n", RBX, RAX);
        fprintf_c(dst, "\tmov %s, [%s + %d]\n", RAX, RSP, -1 * COMPILER_WORDSIZE * offset);
        fprintf_c(dst, "\txor %s, %s\n", RDX, RDX);
        fprintf_c(dst, "\tdiv %s\n", RBX);
        fprintf_c(dst, "\tshl %s, 6\n", RAX);
        if (__comp_trace__) { printf("DONE compile_div\n"); }
        break;
    case OP_AND:
        if (__comp_trace__) { printf("compile_and\n"); }
        compile_expr(ast->left, dst, table);
        assert_bool(dst);
        fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * offset, RAX);
        compile_expr(ast->right, dst, table);
        assert_bool(dst);
        fprintf_c(dst, "\tmov %s, %s\n", RBX, RAX);
        fprintf_c(dst, "\tmov %s, [%s + %d]\n", RAX, RSP, -1 * COMPILER_WORDSIZE * offset);
        fprintf_c(dst, "\tand %s, %s\n", RAX, RBX);
        if (__comp_trace__) { printf("DONE compile_and\n"); }
        break;
    case OP_LOR:
        if (__comp_trace__) { printf("compile_or\n"); }
        compile_expr(ast->left, dst, table);
        assert_bool(dst);
        fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * offset, RAX);
        compile_expr(ast->right, dst, table);
        assert_bool(dst);
        fprintf_c(dst, "\tmov %s, %s\n", RBX, RAX);
        fprintf_c(dst, "\tmov %s, [%s + %d]\n", RAX, RSP, -1 * COMPILER_WORDSIZE * offset);
        fprintf_c(dst, "\tor %s, %s\n", RAX, RBX);
        if (__comp_trace__) { printf("DONE compile_or\n"); }
        break;
    case CMP_SE:
        /* it's complicated */
        compile_expr(ast->left, dst, table);
        assert_type(dst, 16, (255 << 8) | TAG_SYMBOL);
        fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * offset, RAX);
        compile_expr(ast->right, dst, table);
        assert_type(dst, 16, (255 << 8) | TAG_SYMBOL);
        fprintf_c(dst, "\tmov %s, %s\n", RSI, RAX);
        fprintf_c(dst, "\tshr %s, %d\n", RSI, 16);
        fprintf_c(dst, "\tmov %s, [%s + %d]\n", RDI, RSP, -1 * COMPILER_WORDSIZE * offset);
        fprintf_c(dst, "\tshr %s, %d\n", RDI, 16);
        fprintf_c(dst, "\tsub %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
        fprintf_c(dst, "\tcall bytes_equal\n");
        fprintf_c(dst, "\tadd %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
        break;
    case CMP_RE:
        /* it's complicated */
        compile_expr(ast->left, dst, table);
        assert_type(dst, 3, TAG_STRING);
        fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * offset, RAX);
        compile_expr(ast->right, dst, table);
        assert_type(dst, 3, TAG_STRING);
        fprintf_c(dst, "\tmov %s, %s\n", RSI, RAX);
        fprintf_c(dst, "\txor %s, %d\n", RSI, TAG_STRING);
        fprintf_c(dst, "\tmov %s, [%s + %d]\n", RDI, RSP, -1 * COMPILER_WORDSIZE * offset);
        fprintf_c(dst, "\txor %s, %d\n", RDI, TAG_STRING);
        fprintf_c(dst, "\tsub %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
        fprintf_c(dst, "\tcall bytes_equal\n");
        fprintf_c(dst, "\tadd %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
        break;
    case OP_CNS:
        if (__comp_trace__) { printf("compile_cons\n"); }
        {
            push_scope(table);
            int __offset = bind_(table, dst, 0, 0, "*****");
            compile_expr(ast->left, dst, table);
            fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * offset, RAX);
            compile_expr(ast->right, dst, table);
            //fprintf_c(dst, "\tmov %s, %s\n", R15, RSP);
            //fprintf_c(dst, "\tmov %s, %d\n", R14, COMPILER_WORDSIZE * (lexicon_offset(table) + 0));
            
            fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * __offset, RAX);
            fprintf_c(dst, "\tsub %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            fprintf_c(dst, "\tcall gcalloc\n");
            fprintf_c(dst, "\tadd %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            fprintf_c(dst, "\tmov %s, [%s + %d]\n", RAX, RSP, -1 * COMPILER_WORDSIZE * __offset);
            fprintf_c(dst, "\tmov %s, [%s + %d]\n", RCX, RSP, -1 * COMPILER_WORDSIZE * offset);
            fprintf_c(dst, "\tmov [%s], %s\n", RDI, RCX);
            fprintf_c(dst, "\tmov [%s + %d], %s\n", RDI, COMPILER_WORDSIZE, RAX);
            fprintf_c(dst, "\tmov %s, %s\n", RAX, RDI);
            fprintf_c(dst, "\tor %s, %d\n", RAX, TAG_LIST);
            pop_scope_(table, dst);
        }
        if (__comp_trace__) { printf("DONE compile_cons\n"); }
        break;
    case CMP_GT:
        if (__comp_trace__) { printf("compile_greaterthan\n"); }
        compile_compare(ast, dst, table, "jg", 0, offset);
        if (__comp_trace__) { printf("DONE compile_greaterthan\n"); }
        break;
    case CMP_GE:
        if (__comp_trace__) { printf("compile_greaterequal\n"); }
        compile_compare(ast, dst, table, "jge", 0, offset);
        if (__comp_trace__) { printf("DONE compile_greaterequal\n"); }
        break;
    case CMP_LE:
        if (__comp_trace__) { printf("compile_lessequal\n"); }
        compile_compare(ast, dst, table, "jle", 0, offset);
        if (__comp_trace__) { printf("DONE compile_lessequal\n"); }
        break;
    case CMP_LT:
        if (__comp_trace__) { printf("compile_lessthan\n"); }
        compile_compare(ast, dst, table, "jl", 0, offset);
        if (__comp_trace__) { printf("DONE compile_lessthan\n"); }
        break;
    case CMP_EQ:
        if (__comp_trace__) { printf("compile_equals\n"); }
        compile_compare(ast, dst, table, "jne", 1, offset);
        if (__comp_trace__) { printf("DONE compile_equals\n"); }
        break;
    case CMP_NE:
        if (__comp_trace__) { printf("compile_notequals\n"); }
        compile_compare(ast, dst, table, "je", 1, offset);
        if (__comp_trace__) { printf("DONE compile_notequals\n"); }
        break;
    default:
        compile_error(0, 0, "fatal error occurred.\n");
    }
    pop_scope_(table, dst);
    __tail__ = _tail;
}

void compile_compare (node_t* ast, FILE* dst, lexicon_t* table, char* ins, int eq, int offset) {
    int _tail = __tail__;
    __tail__ = 0;
    char sym[30] = {0};
    compile_expr(ast->left, dst, table);
    if (!eq) {
        assert_integer(dst);
    }
    fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * offset, RAX);
    compile_expr(ast->right, dst, table);
    if (!eq) {
        assert_integer(dst);
    }
    fprintf_c(dst, "\tsub %s, [%s + %d]\n", RAX, RSP, -1 * COMPILER_WORDSIZE * offset);
    fprintf_c(dst, "\tcmp %s, 0\n", RAX);
    gensym(&sym[0], "comp");
    fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(0));
    fprintf_c(dst, "\t%s %s\n", ins, &sym[0]);
    fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(1));
    fprintf_c(dst, "%s:\n", &sym[0]);
    __tail__ = _tail;
}

void compile_unary (node_t* ast, FILE* dst, lexicon_t* table) {
    char sym[30] = {0};
    int _tail = __tail__;
    __tail__ = 0;
    switch (ast->otype) {
    case OP_IDT:
        compile_expr(ast->left, dst, table);
        break;
    case OP_PRT:
        compile_expr(ast->left, dst, table);
        fprintf_c(dst, "\tmov %s, %s\n", RDI, RAX);
        fprintf_c(dst, "\tsub %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
        fprintf_c(dst, "\tcall print_result_\n");
        fprintf_c(dst, "\tadd %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
        break;
    case OP_NOT:
        compile_expr(ast->left, dst, table);
        gensym(&sym[0], "chk");
        assert_bool(dst);
        fprintf_c(dst, "\tcmp %s, %lu\n", RAX, mask_bool(0));
        fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(0));
        fprintf_c(dst, "\tjne %s\n", &sym[0]);
        fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(1));
        fprintf_c(dst, "%s:\n", &sym[0]);
        break;
    case CHKSTR:
        compile_expr(ast->left, dst, table);
        gensym(&sym[0], "chk");
        fprintf_c(dst, "\tmov %s, %s\n", RBX, RAX);
        fprintf_c(dst, "\tand %s, %d\n", RBX, 7);
        fprintf_c(dst, "\tcmp %s, %d\n", RBX, TAG_STRING);
        fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(0));
        fprintf_c(dst, "\tjne %s\n", &sym[0]);
        fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(1));
        fprintf_c(dst, "%s:\n", &sym[0]);
        break;
    case CHKCNS:
        compile_expr(ast->left, dst, table);
        gensym(&sym[0], "chk");
        fprintf_c(dst, "\tmov %s, %s\n", RBX, RAX);
        fprintf_c(dst, "\tand %s, %d\n", RBX, 7);
        fprintf_c(dst, "\tcmp %s, %d\n", RBX, TAG_LIST);
        fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(0));
        fprintf_c(dst, "\tjne %s\n", &sym[0]);
        fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(1));
        fprintf_c(dst, "%s:\n", &sym[0]);
        break;
    case CHKLST:
        compile_expr(ast->left, dst, table);
        char sympp[30] = {0};
        gensym(&sym[0], "chk");
        gensym(&sympp[0], "chkp");
        fprintf_c(dst, "\tmov %s, %s\n", RBX, RAX);
        fprintf_c(dst, "\tand %s, %d\n", RBX, 7);
        fprintf_c(dst, "\tcmp %s, %d\n", RBX, TAG_LIST);
        fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(0));
        fprintf_c(dst, "\tjne %s\n", &sym[0]);
        fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(1));
        fprintf_c(dst, "\tjmp %s\n", &sympp[0]);
        fprintf_c(dst, "%s:\n", &sym[0]);
        fprintf_c(dst, "\tmov %s, %s\n", RBX, RAX);
        fprintf_c(dst, "\tand %s, %d\n", RBX, TAG_BITS);
        fprintf_c(dst, "\tcmp %s, %d\n", RBX, TAG_ELIST);
        fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(0));
        fprintf_c(dst, "\tjne %s\n", &sym[0]);
        fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(1));
        fprintf_c(dst, "%s:\n", &sympp[0]);
        break;
    case CHKELS:
        compile_expr(ast->left, dst, table);
        gensym(&sym[0], "chk");
        fprintf_c(dst, "\tmov %s, %s\n", RBX, RAX);
        fprintf_c(dst, "\tand %s, %d\n", RBX, TAG_BITS);
        fprintf_c(dst, "\tcmp %s, %d\n", RBX, TAG_ELIST);
        fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(0));
        fprintf_c(dst, "\tjne %s\n", &sym[0]);
        fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(1));
        fprintf_c(dst, "%s:\n", &sym[0]);
        break;
    case CHKINT:
        compile_expr(ast->left, dst, table);
        gensym(&sym[0], "chk");
        fprintf_c(dst, "\tmov %s, %s\n", RBX, RAX);
        fprintf_c(dst, "\tand %s, %d\n", RBX, TAG_BITS);
        fprintf_c(dst, "\tcmp %s, %d\n", RBX, TAG_INT);
        fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(0));
        fprintf_c(dst, "\tjne %s\n", &sym[0]);
        fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(1));
        fprintf_c(dst, "%s:\n", &sym[0]);
        break;
    case CHKBOO:
    case CHKFLG:
        compile_expr(ast->left, dst, table);
        gensym(&sym[0], "chk");
        fprintf_c(dst, "\tmov %s, %s\n", RBX, RAX);
        fprintf_c(dst, "\tand %s, %d\n", RBX, TAG_BITS);
        fprintf_c(dst, "\tcmp %s, %d\n", RBX, TAG_FLAG);
        fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(0));
        fprintf_c(dst, "\tjne %s\n", &sym[0]);
        fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(1));
        fprintf_c(dst, "%s:\n", &sym[0]);
        break;
    case CHKSYM:
        compile_expr(ast->left, dst, table);
        gensym(&sym[0], "chk");
        fprintf_c(dst, "\tmov %s, %s\n", RBX, RAX);
        fprintf_c(dst, "\tand %s, %d\n", RBX, 7);
        fprintf_c(dst, "\tcmp %s, %d\n", RBX, TAG_SYMBOL);
        fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(0));
        fprintf_c(dst, "\tjne %s\n", &sym[0]);
        fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(1));
        fprintf_c(dst, "%s:\n", &sym[0]);
        break;
    case CHKPRC:
        compile_expr(ast->left, dst, table);
        gensym(&sym[0], "chk");
        fprintf_c(dst, "\tmov %s, %s\n", RBX, RAX);
        fprintf_c(dst, "\tand %s, %d\n", RBX, 7);
        fprintf_c(dst, "\tcmp %s, %d\n", RBX, TAG_PROC);
        fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(0));
        fprintf_c(dst, "\tjne %s\n", &sym[0]);
        fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(1));
        fprintf_c(dst, "%s:\n", &sym[0]);
        break;
    case CHKBOX:
        compile_expr(ast->left, dst, table);
        gensym(&sym[0], "chk");
        fprintf_c(dst, "\tmov %s, %s\n", RBX, RAX);
        fprintf_c(dst, "\tand %s, %d\n", RBX, 7);
        fprintf_c(dst, "\tcmp %s, %d\n", RBX, TAG_BOX);
        fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(0));
        fprintf_c(dst, "\tjne %s\n", &sym[0]);
        fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(1));
        fprintf_c(dst, "%s:\n", &sym[0]);
        break;
    case CHKBYT:
        compile_expr(ast->left, dst, table);
        gensym(&sym[0], "chk");
        fprintf_c(dst, "\tmov %s, %s\n", RBX, RAX);
        fprintf_c(dst, "\tand %s, %d\n", RBX, TAG_BITS);
        fprintf_c(dst, "\tcmp %s, %d\n", RBX, TAG_BYTES);
        fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(0));
        fprintf_c(dst, "\tjne %s\n", &sym[0]);
        fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(1));
        fprintf_c(dst, "%s:\n", &sym[0]);
        break;
    case OP_LEN:
        /* long, i think */
        gensym(&sym[0], "lenloop");
        compile_expr(ast->left, dst, table);
        fprintf_c(dst, "\tmov %s, %s\n", R10, RAX);
        fprintf_c(dst, "\tmov %s, %d\n", RAX, 0);
        fprintf_c(dst, "\t%s:\n", &sym[0]);
        char sympp2[30] = {0};
        gensym(&sympp2[0], "llaff");
        fprintf_c(dst, "\tcmp %s, %d\n", R10, TAG_ELIST);
        fprintf_c(dst, "\tje %s\n", &sympp2[0]);
        /* assert type to ensure listhood */
        fprintf_c(dst, "\tmov %s, %s\n", RBX, R10);
        fprintf_c(dst, "\tand %s, %d\n", RBX, 7);
        fprintf_c(dst, "\tcmp %s, %d\n", RBX, TAG_LIST);
        fprintf_c(dst, "\tjne %s\n", &sympp2[0]);
        fprintf_c(dst, "\txor %s, %d\n", R10, TAG_LIST);
        fprintf_c(dst, "\tmov %s, [%s + %d]\n", R10, R10, COMPILER_WORDSIZE);
        fprintf_c(dst, "\tadd %s, %d\n", RAX, mask_integer(1));
        fprintf_c(dst, "\tjmp %s\n", &sym[0]);
        fprintf_c(dst, "%s:\n", &sympp2[0]);
        break;
    case OP_ABS:
        if (__comp_trace__) { printf("compile_abs\n"); }
        compile_expr(ast->left, dst, table);
        assert_integer(dst);
        fprintf_c(dst, "\tmov %s, %s\n", RBX, RAX);
        fprintf_c(dst, "\tneg %s\n", RAX);
        fprintf_c(dst, "\tcmovl %s, %s\n", RAX, RBX);
        /* clear RBX, used by error */
        fprintf_c(dst, "\txor %s, %s\n", RBX, RBX);
        if (__comp_trace__) { printf("DONE compile_abs\n"); }
        break;
    case OP_NEG:
        if (__comp_trace__) { printf("compile_neg\n"); }
        compile_expr(ast->left, dst, table);
        assert_integer(dst);
        fprintf_c(dst, "\tneg %s\n", RAX);
        if (__comp_trace__) { printf("DONE compile_neg\n"); }
        break;
    case OP_INC:
        if (__comp_trace__) { printf("compile_add1\n"); }
        compile_expr(ast->left, dst, table);
        assert_integer(dst);
        fprintf_c(dst, "\tadd %s, %li\n", RAX, mask_integer(1));
        if (__comp_trace__) { printf("DONE compile_add1\n"); }
        break;
    case OP_DEC:
        if (__comp_trace__) { printf("compile_sub1\n"); }
        compile_expr(ast->left, dst, table);
        assert_integer(dst);
        fprintf_c(dst, "\tsub %s, %li\n", RAX, mask_integer(1));
        if (__comp_trace__) { printf("DONE compile_sub1\n"); }
        break;
    case CMP_ZR:
        if (__comp_trace__) { printf("compile_zero\n"); }
        gensym(&sym[0], "zero");
        compile_expr(ast->left, dst, table);
        assert_integer(dst);
        fprintf_c(dst, "\tcmp %s, %d\n", RAX, 0);
        fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(0));
        fprintf_c(dst, "\tjne %s\n", &sym[0]);
        fprintf_c(dst, "\tmov %s, %lu\n", RAX, mask_bool(1));
        fprintf_c(dst, "%s:\n", &sym[0]);
        if (__comp_trace__) { printf("DONE compile_zero\n"); }
        break;
    case OP_BOX:
        if (__comp_trace__) { printf("compile_box\n"); }
        //fprintf_c(dst, "\tmov %s, %s\n", R15, RSP);
        //fprintf_c(dst, "\tmov %s, %d\n", R14, COMPILER_WORDSIZE * (lexicon_offset(table) + 0));
        {
            push_scope(table);
            int __offset = bind_(table, dst, 0, 0, "******");
            compile_expr(ast->left, dst, table);
            fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * __offset, RAX);
            fprintf_c(dst, "\tsub %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            fprintf_c(dst, "\tcall gcalloc\n");
            fprintf_c(dst, "\tadd %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
            fprintf_c(dst, "\tmov %s, [%s + %d]\n", RAX, RSP, -1 * COMPILER_WORDSIZE * __offset);
            fprintf_c(dst, "\tmov [%s], %s\n", RDI, RAX);
            fprintf_c(dst, "\tmov %s, %s\n", RAX, RDI);
            fprintf_c(dst, "\tor %s, %d\n", RAX, TAG_BOX);
            pop_scope_(table, dst);
        }
        if (__comp_trace__) { printf("DONE compile_box\n"); }
        break;
    case OP_UBX:
        if (__comp_trace__) { printf("compile_unbox\n"); }
        compile_expr(ast->left, dst, table);
        assert_type(dst, 3, TAG_BOX);
        fprintf_c(dst, "\txor %s, %d\n", RAX, TAG_BOX);
        fprintf_c(dst, "\tmov %s, [%s]\n", RAX, RAX);
        if (__comp_trace__) { printf("DONE compile_unbox\n"); }
        break;
    case OP_CDR:
        if (__comp_trace__) { printf("compile_cdr\n"); }
        compile_expr(ast->left, dst, table);
        assert_type(dst, 3, TAG_LIST);
        fprintf_c(dst, "\txor %s, %d\n", RAX, TAG_LIST);
        fprintf_c(dst, "\tmov %s, [%s + %d]\n", RAX, RAX, COMPILER_WORDSIZE);
        if (__comp_trace__) { printf("DONE compile_cdr\n"); }
        break;
    case OP_CAR:
        if (__comp_trace__) { printf("compile_car\n"); }
        compile_expr(ast->left, dst, table);
        assert_type(dst, 3, TAG_LIST);
        fprintf_c(dst, "\txor %s, %d\n", RAX, TAG_LIST);
        fprintf_c(dst, "\tmov %s, [%s]\n", RAX, RAX);
        if (__comp_trace__) { printf("DONE compile_car\n"); }
        break;
    case OP_FUN:
        /* we need to assert sval is a function */
        
        break;
    default:
        compile_error(ast->l, ast->c, "fatal error occured.");
    }
    __tail__ = _tail;
}

void compile_if (node_t* ast, FILE* dst, lexicon_t* table) {
    if (__comp_trace__) { printf("compile_if\n"); }
    char sym1[30] = {0};
    char sym2[30] = {0};
    gensym(&sym1[0], "if");
    gensym(&sym2[0], "if");
    int _tail = __tail__;
    __tail__ = 0;
    compile_expr(ast->cond, dst, table);
    __tail__ = _tail;
    assert_bool(dst);
    fprintf_c(dst, "\tcmp %s, %lu\n", RAX, mask_bool(0));
    fprintf_c(dst, "\tjne %s\n", &sym1[0]);
    compile_expr(ast->right, dst, table);
    fprintf_c(dst, "\tjmp %s\n", &sym2[0]);
    fprintf_c(dst, "%s:\n", &sym1[0]);
    compile_expr(ast->left, dst, table);
    fprintf_c(dst, "%s:\n", &sym2[0]);
    if (__comp_trace__) { printf("DONE compile_if\n"); }
}

void compile_cond (node_t* ast, FILE* dst, int* off, char* el, char* af, lexicon_t* table) {
    if (ast->clauses == NULL || ast->clauses->clauses == NULL || ast->clauses->len == *off) {
        //printf("cond else comp\n");
        if (__comp_trace__) { printf("compile_else\n"); }
        compile_expr(ast->cond, dst, table);
        fprintf_c(dst, "%s:\n", af);
        if (__comp_trace__) { printf("DONE compile_else\n"); }
        return;
    }
    if (__comp_trace__) { printf("compile_cond_clause\n"); }
    //printf("cond clause comp\n");
    char _sym[30] = {0};
    char* sym;
    if (*off < (ast->clauses->len - 1)) {
        gensym(&_sym[0], "cond");
        sym = &_sym[0];
    }
    else {
        sym = el;
    }

    node_t clause = ast->clauses->clauses[*off];
    int _tail = __tail__;
    __tail__ = 0;
    compile_expr(clause.left, dst, table);
    __tail__ = _tail;
    assert_bool(dst);
    fprintf_c(dst, "\tcmp %s, %lu\n", RAX, mask_bool(1));
    fprintf_c(dst, "\tjne %s\n", sym);
    compile_expr(clause.right, dst, table);
    fprintf_c(dst, "\tjmp %s\n", af);
    fprintf_c(dst, "%s:\n", sym);
    (*off)++;
    if (__comp_trace__) { printf("DONE compile_cond_clause\n"); }
    compile_cond(ast, dst, off, el, af, table);
}

char* gensym (char* buf, char* pre) {
    sprintf(buf, "%s%d", pre, ++__gensym);
    return buf;
}

void buf_grow (char** buf, int* len, int* cap, int sz) {
    char* new_buf;
    if (*len == 0) {
        *cap = sz;
        *buf = malloc(sz + 1);
    }
    else {
        *cap *= 2;
        new_buf = malloc(*cap + 1);
        memcpy((uint8_t*) new_buf, (uint8_t*) *buf, *len);
        free(*buf);
        *buf = new_buf;
    }
    for (int i = *len; i < *cap; i++) {
            (*buf)[i] = 0;
    }
}

void compile_error (int l, int c, char* msg, ...) {
    va_list v;
    va_start(v, msg);
    printf("%d:%d: ", l, c);
    vprintf(msg, v);
    printf("\n");
    va_end(v);
    exit(1);
}

void compile_call (node_t* ast, FILE* dst, lexicon_t* table, int proc) {
    if (__comp_trace__) { printf("compile_call\n"); }
    int _tail = __tail__;
    __tail__ = 0;
    if (_tail) {
        compile_closure_tail_call(ast, dst, table);
    } else {
        compile_closure_call(ast, dst, table);
    }
    __tail__ = _tail;
    if (__comp_trace__) { printf("DONE compile_call\n"); }
}
/*
void compile_call (node_t* ast, FILE* dst, lexicon_t* table, int proc) {
    // we need to get stack size 
    int _tail = __tail__;
    __tail__ = 0;
    int __hax__;
    // hax 
    if (proc) {
        push_scope(table);
        __hax__ = bind_(table, dst, -1, -1, "");
    }

    int stacksize = lexicon_offset();
    int numargs = proc ? 1 : get_args(table, ast->sval);
    
    if (proc) {
        compile_expr(ast->cond, dst, table);
        fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * __hax__, RAX);
    }

    if (!_tail) {
        for (int i = 0; i < ast->clauses->len; i++) {
            compile_expr(&(ast->clauses->clauses[i]), dst, table);
            fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * (stacksize + i + 1), RAX);
        }

        if (!proc) {
            if (numargs == -1) {
                compile_error(ast->l, ast->c, "call to undefined \'%s\'", ast->sval);
            }
            char* label = get_definition(table, ast->sval);
            if (label == NULL) {
                compile_error(ast->l, ast->c, "definition lookup failed: %s", ast->sval);
            }

            if (ast->clauses->len != numargs) {
                compile_error(ast->l, ast->c, "call to \'%s\' expects %d arguments ; got %d", numargs, ast->clauses->len);
            }

            fprintf_c(dst, "\tsub %s, %d\n", RSP, COMPILER_WORDSIZE * (stacksize - 1));
            fprintf_c(dst, "\tcall %s\n", label);
            fprintf_c(dst, "\tadd %s, %d\n", RSP, COMPILER_WORDSIZE * (stacksize - 1));
        } else {
            fprintf_c(dst, "\tmov %s, [%s + %d]\n", RAX, RSP, -1 * COMPILER_WORDSIZE * __hax__);
            // popping scope here would be irresponsible, so we wait.
            assert_type(dst, 3, TAG_PROC);
            fprintf_c(dst, "\txor %s, %d\n", RAX, TAG_PROC);
            fprintf_c(dst, "\tsub %s, %d\n", RSP, COMPILER_WORDSIZE * (stacksize - 1));
            fprintf_c(dst, "\tcall [%s]\n", RAX);
            fprintf_c(dst, "\tadd %s, %d\n", RSP, COMPILER_WORDSIZE * (stacksize - 1));
        }
    } else {
        for (int i = 0; i < ast->clauses->len; i++) {
            compile_expr(&(ast->clauses->clauses[i]), dst, table);
            fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * (ast->clauses->len + i + 1), RAX);
        }

        if (!proc) {
            if (numargs == -1) {
                compile_error(ast->l, ast->c, "call to undefined \'%s\'", ast->sval);
            }

            char* label = get_definition(table, ast->sval);
            if (label == NULL) {
                compile_error(ast->l, ast->c, "definition lookup failed: %s", ast->sval);
            }

            if (ast->clauses->len != numargs) {
                compile_error(ast->l, ast->c, "call to \'%s\' expects %d arguments ; got %d", numargs, ast->clauses->len);
            }

            for (int i = 0; i < ast->clauses->len; i++) {
                fprintf_c(dst, "\tmov %s, [%s + %d]\n", RBX, RSP, -1 * COMPILER_WORDSIZE * (ast->clauses->len + i + 1));
                fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * (i + 1), RBX);
            }

            fprintf_c(dst, "\tjmp %s\n", label);
        } else {
            for (int i = 0; i < ast->clauses->len; i++) {
                fprintf_c(dst, "\tmov %s, [%s + %d]\n", RBX, RSP, -1 * COMPILER_WORDSIZE * (ast->clauses->len + i + 1));
                fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * (i + 1), RBX);
            }
            fprintf_c(dst, "\tmov %s, [%s + %d]\n", RAX, RSP, -1 * COMPILER_WORDSIZE * __hax__);
            assert_type(dst, 3, TAG_PROC);
            fprintf_c(dst, "\txor %s, %d\n", RAX, TAG_PROC);
            fprintf_c(dst, "\tjmp [%s]\n", RAX);
        }
    }

    if (proc) {
        pop_scope_(table, dst);
    }
    __tail__ = _tail;
}*/

void compile_define (node_t* ast, FILE* dst, lexicon_t* table) {
    if (__comp_trace__) { printf("compile_define %s\n", ast->sval); }
    char* label = get_definition(table, ast->sval);
    fprintf_c(dst, "\nalign 16\n");
    fprintf_c(dst, "%s:\n", label);
    int offset;

    push_scope(table);
     /* iterate over everything to build parameter bindings */
    for (int i = 0; i < ast->clauses->len; i++) {
        //printf("let: %s\n", ast->clauses->clauses[i].sval);
        __ignorezero__ = 1;
        offset = bind_(table, dst, ast->clauses->clauses[i].l, ast->clauses->clauses[i].c, ast->clauses->clauses[i].sval);
        /* all calls to bind now do gcstack pushes */
        __ignorezero__ = 0;
    }

    /* compile correctly, dammit! */
    //__tail__ = 0;
    //node_t* recurn = ast->right;
    //while(recurn != NULL) {
    //    compile_expr(recurn, dst, table);
    //    recurn = recurn->right;
    //}

    /* what's next could be tail call */
    __tail__ = 1;
    compile_expr(ast->left, dst, table);
    __tail__ = 0;
    pop_scope_(table, dst);
    fprintf_c(dst, "\tret\n");
    /* pop scope now performs scope pop */
    if (__comp_trace__) { printf("DONE compile_define %s\n", ast->sval); }
}

void compile_closure (node_t* ast, FILE* dst, lexicon_t* table) {
    if (__comp_trace__) { printf("compile_closure\n"); }
    /* iterate over everything to build parameter bindings */
    lexicon_t freevars;
    new_lexicon(&freevars);
    freevars.defs = table->defs;
    push_scope(&freevars);
    scope_t* scope = &(freevars.scopes[freevars.len - 1]);
    for (int i = 0; i < ast->clauses->len; i++) {
        sbind(scope, ast->clauses->clauses[i].sval);
    }

    /* capture */
    scope_t* fvs = scope_a();
    lfree(ast->left, dst, table, fvs);
    if (__comp_trace__)
    for (int i = 0; i < fvs->len; i++) {
        printf("freevars: %s", fvs->vars[i].name);
        if (i < (fvs->len - 1)) {
            printf(", ");
        } else {
            printf("\n");
        }
    }
    ast->ival = fvs->len;
    push_scope_a(&freevars, scope, fvs);
    if (__comp_trace__) printf("IGNORE EVERYTHING AND LOOK: %d\n", lexicon_offset(&freevars));
    if (__comp_trace__) printf("IGNORE EVERYTHING AND LOOK: %d, %d\n", scope->len, scope->offc);
    /* compilation */
    __tail__ = 1;
    fprintf_c(dst, "align 16\n");
    fprintf_c(dst, "%s:\n", ast->sval);
    /* actually, we need to ensure the frame is update to include implicit and explicit arguments */
    fprintf_c(dst, "\tmov %s, [%s]\n", R13, "__gcframe__");
    //fprintf_c(dst, "\tsub %s, %s\n", R11, R13);
    fprintf_c(dst, "\tadd %s, %d\n", R13, (lexicon_offset(&freevars) - 1) * COMPILER_WORDSIZE);
    fprintf_c(dst, "\tmov [%s], %s\n", "__gcframe__", R13);
    compile_expr(ast->left, dst, &freevars);
    __tail__ = 0;
    fprintf_c(dst, "\tmov %s, [%s]\n", R13, "__gcframe__");
    //fprintf_c(dst, "\tsub %s, %s\n", R11, R13);
    fprintf_c(dst, "\tsub %s, %d\n", R13, (lexicon_offset(&freevars) - 1) * COMPILER_WORDSIZE);
    fprintf_c(dst, "\tmov [%s], %s\n", "__gcframe__", R13);
    fprintf_c(dst, "\tret\n\n");

    /* cleanup */
    scope_d(fvs);
    destroy_lexicon(&freevars);
    if (__comp_trace__) { printf("DONE compile_closure\n"); }
}

void compile_closure_create (node_t* ast, FILE* dst, lexicon_t* table) {
    if (__comp_trace__) { printf("compile_closure_create\n"); }
    /* recalculate freevars */
    scope_t* fvs = scope_a();
    /* pass in whole ast; freevars CANNOT include lambda formals. */
    lfree(ast, dst, table, fvs);
    if (__comp_trace__)
    for (int i = 0; i < fvs->len; i++) {
        printf("freevars: %s", fvs->vars[i].name);
        if (i < (fvs->len - 1)) {
            printf(", ");
        } else {
            printf("\n");
        }
    }

    /* setup call to allocator/collector */
    ///fprintf_c(dst, "\tmov %s, %s\n", R15, RSP);
    fprintf_c(dst, "\tmov %s, %d\n", R10, TAG_ELIST);
    /* create closure environment */
    push_scope(table);
    int __offset = bind_(table, dst, 0, 0, "*******");
    if (__comp_trace__) { printf("compile_closure_create (CLOSURE_SAVE)\n"); }
    for (int i = (fvs->len - 1); i >= 0; i--) {
        //fprintf_c(dst, "\tmov %s, %d\n", R14, COMPILER_WORDSIZE * (lexicon_offset(table) + 0));
        fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * __offset, R10);
        fprintf_c(dst, "\tsub %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
        fprintf_c(dst, "\tcall gcalloc\n"); /* allocator call */
        fprintf_c(dst, "\tadd %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
        fprintf_c(dst, "\tmov %s, [%s + %d]\n", R10, RSP, -1 * COMPILER_WORDSIZE * __offset);
        fprintf_c(dst, "\tmov %s, %s\n", R9, RDI);
        fprintf_c(dst, "\tmov %s, [%s + %d]\n", R8, RSP, -1 * COMPILER_WORDSIZE * (0 + lookup(table, fvs->vars[i].name)));
        fprintf_c(dst, "\tmov [%s], %s\n", R9, R8);
        fprintf_c(dst, "\tmov [%s + %d], %s\n", R9, COMPILER_WORDSIZE, R10);
        fprintf_c(dst, "\tor %s, %d\n", R9, TAG_LIST);
        fprintf_c(dst, "\tmov %s, %s\n", R10, R9);
    } 

    pop_scope_(table, dst);
    push_scope(table);
    __offset = bind_(table, dst, 0, 0, "*******");
    int __r10off = bind_(table, dst, 0, 0, "********");
    /* R10 stores the environment list */
    if (__comp_trace__) { printf("compile_closure_create (CLOSURE_MAIN)\n"); }
    fprintf_c(dst, "\tlea %s, [%s]\n", RAX, ast->sval);
    fprintf_c(dst, "\tadd %s, %d\n", RAX, TAG_PROC);
    //fprintf_c(dst, "\tmov %s, 1\n", RBX);
    //fprintf_c(dst, "\tshl %s, 63\n", RBX);
    //fprintf_c(dst, "\tor %s, %s\n", RAX, RBX);
    fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * __offset, RAX);
    fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * __r10off, R10);
    fprintf_c(dst, "\tsub %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
    fprintf_c(dst, "\tcall gcalloc\n"); /* main allocation call */
    fprintf_c(dst, "\tadd %s, %d\n", RSP, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
    fprintf_c(dst, "\tmov %s, [%s + %d]\n", R10, RSP, -1 * COMPILER_WORDSIZE * __r10off);
    fprintf_c(dst, "\tmov %s, [%s + %d]\n", RAX, RSP, -1 * COMPILER_WORDSIZE * __offset);
    fprintf_c(dst, "\tmov [%s], %s\n", RDI, RAX);
    fprintf_c(dst, "\tmov [%s + %d], %s\n", RDI, COMPILER_WORDSIZE, R10);

    if (__comp_trace__) { printf("compile_closure_create (CLOSURE_TAG)\n"); }
    fprintf_c(dst, "\tmov %s, %s\n", RAX, RDI);
    fprintf_c(dst, "\tor %s, %d\n", RAX, TAG_PROC);
    scope_d(fvs);
    pop_scope_(table, dst);
    if (__comp_trace__) { printf("DONE compile_closure_create\n"); }
}

void compile_closure_envcopy (FILE *dst) {
    if (__comp_trace__) { printf("compile_closure_envcopy\n"); }
    char sym1[30] = {0};
    gensym(&sym1[0], "envcopy");
    char sym2[30] = {0};
    gensym(&sym2[0], "envcopy");
    fprintf_c(dst, "\tmov %s, [%s + %d]\n", R8, RAX, COMPILER_WORDSIZE);
    fprintf_c(dst, "%s:\n", &sym1[0]);
    fprintf_c(dst, "\tcmp %s, %d\n", R8, TAG_ELIST);
    fprintf_c(dst, "\tje %s\n", &sym2[0]);
    fprintf_c(dst, "\txor %s, %d\n", R8, TAG_LIST);
    fprintf_c(dst, "\tmov %s, [%s]\n", RBX, R8); /* data */
    fprintf_c(dst, "\tmov [%s], %s\n", RCX, RBX);
    fprintf_c(dst, "\tmov %s, [%s + %d]\n", R9, R8, COMPILER_WORDSIZE); /* copy next element into r8 */
    fprintf_c(dst, "\tmov %s, %s\n", R8, R9); /* then move to r8 */
    fprintf_c(dst, "\tsub %s, %d\n", RCX, COMPILER_WORDSIZE);
    fprintf_c(dst, "\tjmp %s\n", &sym1[0]);
    fprintf_c(dst, "%s:\n", &sym2[0]);
    if (__comp_trace__) { printf("DONE compile_closure_envcopy\n"); }
}

void compile_letrec (node_t* ast, FILE* dst, lexicon_t* table) {

}

void compile_closure_call (node_t* ast, FILE* dst, lexicon_t* table) {
    if (__comp_trace__) { printf("compile_closure_call\n"); }

    /* we need to reserve space for marker and $ra */
    push_scope(table);
    //int __ignorezero = __ignorezero__;
    //__ignorezero__ = 1;
    int __marker_offset = bind_(table, dst, 0, 0, "***marker");
    /* $ra dummy */
    bind_(table, dst, 0, 0, "***ra");
    //__ignorezero__ = __ignorezero;

    /* prepare stack for argument construction */
    push_scope(table);
    int stacksize = lexicon_offset(table);
    for (int i = 0; i < ast->clauses->len; i++) {
        bind_(table, dst, 0, 0, "***#");
    }

    int __hax__ = bind_(table, dst, 0, 0, ""); /* at veeery end. */

    if (ast->cond == NULL && ast->sval != NULL) {
        /* still might be a variable storing a closure */
        /* then we "fake" a closure with a def */
        if (get_definition(table, ast->sval) == NULL) {
            //compile_error(ast->l, ast->c, "definition lookup failed: %s", ast->sval);
            /* practice now is to assume variable */
            int offset = lookup(table, ast->sval);
            if (offset == LEXICON_NOTFOUND) {
                compile_error(ast->l, ast->c, "undefined variable or function: %s\n", ast->sval);
            }
            if (__comp_trace__) { printf("compile_closure_call (BEGIN, VARIABLE)\n"); }
            fprintf_c(dst, "\tmov %s, [%s + %d]\n", RAX, RSP, -1 * COMPILER_WORDSIZE * offset);
        } else {
            if (__comp_trace__) { printf("compile_closure_call (BEGIN, DEFINE)\n"); }
            fprintf_c(dst, "\tlea %s, [%s%s]\n", RAX, "dummy", get_definition(table, ast->sval));
            fprintf_c(dst, "\tor %s, %d\n", RAX, TAG_PROC);
        }
    } else {
        if (__comp_trace__) { printf("compile_closure_call (BEGIN, CLOSURE)\n"); }
        compile_expr(ast->cond, dst, table);
    }
    
    fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * __hax__, RAX);

    for (int i = 0; i < ast->clauses->len; i++) {
        if (__comp_trace__) { printf("compile_closure_call (COMPILE ARGS - %d)\n", i); }
        compile_expr(&(ast->clauses->clauses[i]), dst, table);
        fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * (stacksize + i), RAX);
    }

    if (__comp_trace__) { printf("compile_closure_call (RESTORE, CHECK TYPE, UPDATE STACK)\n"); }
    fprintf_c(dst, "\tmov %s, [%s + %d]\n", RAX, RSP, -1 * COMPILER_WORDSIZE * __hax__);

    assert_type(dst, 3, TAG_PROC);
    fprintf_c(dst, "\txor %s, %d\n", RAX, TAG_PROC);

    /* insert marker prior to subbing */
    fprintf_c(dst, "\tmov %s, %d\n", R11, 7);
    fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * __marker_offset * COMPILER_WORDSIZE, R11);

    fprintf_c(dst, "\tsub %s, %d\n", RSP, COMPILER_WORDSIZE * (stacksize - 2));

    if (__comp_trace__) { printf("compile_closure_call (COPY ENVIRONMENT)\n"); }
    fprintf_c(dst, "\tmov %s, %s\n", RCX, RSP);
    fprintf_c(dst, "\tadd %s, %d\n", RCX, -1 * COMPILER_WORDSIZE * (2 + ast->clauses->len));
    compile_closure_envcopy(dst);

    if (__comp_trace__) { printf("compile_closure_call (CALL)\n"); }
    fprintf_c(dst, "\tmov %s, [%s]\n", R8, RAX);
    fprintf_c(dst, "\txor %s, %d\n", R8, TAG_PROC);
    //fprintf_c(dst, "\tmov %s, 1\n", RBX);
    //fprintf_c(dst, "\tshl %s, 63\n", RBX);
    //fprintf_c(dst, "\txor %s, %s\n", R8, RBX);
    /* remove __hax__ and arguments from frame */
    pop_scope_(table, dst);

    fprintf_c(dst, "\tcall %s\n", R8);

    /* remove marker and $ra from frame */
    pop_scope_(table, dst);

    fprintf_c(dst, "\tadd %s, %d\n", RSP, COMPILER_WORDSIZE * (stacksize - 2));

    if (__comp_trace__) { printf("DONE compile_closure_call\n"); }
}

/* LOTS OF FIXES NEEDED, NO CLUE IF IT STILL WORKS! */
void compile_closure_tail_call (node_t* ast, FILE* dst, lexicon_t* table) {
    //sleep(1);

    if (__comp_trace__) { printf("compile_closure_tail_call\n"); }

    /* prepare stack for argument construction */
    push_scope(table);
    int stacksize = lexicon_offset(table);
    for (int i = 0; i < ast->clauses->len; i++) {
        bind_(table, dst, 0, 0, "***#");
    }

    int __hax__ = bind_(table, dst, 0, 0, ""); /* at veeery end. */

    if (ast->cond == NULL && ast->sval != NULL) {
        /* still might be a variable storing a closure */
        /* then we "fake" a closure with a def */
        if (get_definition(table, ast->sval) == NULL) {
            //compile_error(ast->l, ast->c, "definition lookup failed: %s", ast->sval);
            /* practice now is to assume variable */
            int offset = lookup(table, ast->sval);
            if (offset == LEXICON_NOTFOUND) {
                compile_error(ast->l, ast->c, "undefined variable or function: %s\n", ast->sval);
            }
            if (__comp_trace__) { printf("compile_closure_tail_call (BEGIN, VARIABLE)\n"); }
            fprintf_c(dst, "\tmov %s, [%s + %d]\n", RAX, RSP, -1 * COMPILER_WORDSIZE * offset);
        } else {
            if (__comp_trace__) { printf("compile_closure_tail_call (BEGIN, DEFINE)\n"); }
            fprintf_c(dst, "\tlea %s, [%s%s]\n", RAX, "dummy", get_definition(table, ast->sval));
            fprintf_c(dst, "\tor %s, %d\n", RAX, TAG_PROC);
        }
    } else {
        if (__comp_trace__) { printf("compile_closure_tail_call (BEGIN, CLOSURE)\n"); }
        compile_expr(ast->cond, dst, table);
    }

    fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * __hax__, RAX);

    for (int i = 0; i < ast->clauses->len; i++) {
        if (__comp_trace__) { printf("compile_closure_tail_call (COMPILE ARGS - %d)\n", i + 1); }
        compile_expr(&(ast->clauses->clauses[i]), dst, table);
        fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * (stacksize + i), RAX);
    }

    if (__comp_trace__) { printf("compile_closure_tail_call (RESTORE, CHECK TYPE)\n"); }
    fprintf_c(dst, "\tmov %s, [%s + %d]\n", RAX, RSP, -1 * COMPILER_WORDSIZE * __hax__);
    assert_type(dst, 3, TAG_PROC);
    fprintf_c(dst, "\txor %s, %d\n", RAX, TAG_PROC);


    for (int i = 0; i < ast->clauses->len; i++) {
        if (__comp_trace__) { printf("compile_closure_tail_call (MOVE ARGS - %d)\n", i + 1); }
        fprintf_c(dst, "\tmov %s, [%s + %d]\n", RBX, RSP, -1 * COMPILER_WORDSIZE * (stacksize + i));
        fprintf_c(dst, "\tmov [%s + %d], %s\n", RSP, -1 * COMPILER_WORDSIZE * (i + 1), RBX);
    }

    if (__comp_trace__) { printf("compile_closure_tail_call (COPY ENVIRONMENT)\n"); }    
    fprintf_c(dst, "\tmov %s, %s\n", RCX, RSP);
    fprintf_c(dst, "\tadd %s, %d\n", RCX, -1 * COMPILER_WORDSIZE * (1 + ast->clauses->len));
    compile_closure_envcopy(dst);

    /* we now pop the scope prior to jumping */
    if (__comp_trace__) { printf("compile_closure_tail_call (CALL)\n"); }
    pop_scope_(table, dst);
    fprintf_c(dst, "\tmov %s, [%s]\n", R8, RAX);
    fprintf_c(dst, "\txor %s, %d\n", R8, TAG_PROC);
    //fprintf_c(dst, "\tmov %s, 1\n", RBX);
    //fprintf_c(dst, "\tshl %s, 63\n", RBX);
    //fprintf_c(dst, "\txor %s, %s\n", R8, RBX);
    /* before jump, we need to remove the entire scope. */
    /* this includes all locals and args. */
    /* they will be rescoped later. */
    fprintf_c(dst, "\tmov %s, [%s]\n", R11, "__gcframe__");
    fprintf_c(dst, "\tsub %s, %d\n", R11, COMPILER_WORDSIZE * (lexicon_offset(table) - 1));
    fprintf_c(dst, "\tmov [%s], %s\n", "__gcframe__", R11);
    fprintf_c(dst, "\tjmp %s\n", R8);
    
    if (__comp_trace__) { printf("DONE compile_closure_tail_call\n"); }
}

void lsapp (nodelist_t* ls, node_t* app) {
    if (2 * ls->len >= ls->cap) {
        if (ls->len == 0) {
            node_t** nls = malloc(16 * sizeof(node_t*));
            ls->cap = 16;
            ls->ls = nls; 
        } else {
            node_t** nls = malloc(2 * ls->cap * sizeof(node_t*));
            memcpy((uint8_t*) nls, (uint8_t*) ls->ls, ls->len * sizeof(node_t*));
            ls->cap = 2 * ls->cap;
            free(ls->ls);
            ls->ls = nls;  
        } 
    }
    ls->ls[ls->len++] = app;
}

void lambdas (node_t* ast, FILE* dst, lexicon_t* table, nodelist_t* ls) {
    if (__comp_trace__) { printf("lambdas: node %s\n", nodespecies(ast)); }
    if (ast == NULL) return;
    switch (ast->ntype) {
    case NODE_BOOLV:
    case NODE_ELIST:
    case NODE_INTEG:
    case NODE_STRNG:
    case NODE_LQFLG:
    case NODE_LQUOT:
    case NODE_LQINT:
    case NODE_LQSTR:
        break;
    case NODE_IUNOP:
        lambdas(ast->left, dst, table, ls);
        break;
    case NODE_IBNOP:
        lambdas(ast->left, dst, table, ls);
        lambdas(ast->right, dst, table, ls);
        break;
    case NODE_IFEXP:
        lambdas(ast->cond, dst, table, ls);
        lambdas(ast->left, dst, table, ls);
        lambdas(ast->right, dst, table, ls);
        break;
    case NODE_LETEX:
        for (int i = 0; i < ast->clauses->len; i++) {
            lambdas(ast->clauses->clauses[i].left, dst, table, ls);
        }
        lambdas(ast->left, dst, table, ls);
        break;
    case NODE_CLOSE:
        /* for convenience, lambda labeling is also done here */
        ast->sval = gensym(calloc(30, sizeof(char)), "lambda");
        if (__comp_trace__) { printf("lambdas found: %s\n", ast->sval); }
        lsapp(ls, ast);
        lambdas(ast->left, dst, table, ls);
        break;
    case NODE_CONDE:
        if (ast->clauses != NULL) {
            for (int i = 0; i < ast->clauses->len; i++) {
                lambdas(ast->clauses->clauses[i].left, dst, table, ls);
                lambdas(ast->clauses->clauses[i].right, dst, table, ls);
            }
        }
        lambdas(ast->cond, dst, table, ls);
        break;
    case NODE_DEFFN:
        lambdas(ast->left, dst, table, ls);
        break;
    case NODE_CALLF:
        for (int i = 0; i < ast->clauses->len; i++) {
            lambdas(&(ast->clauses->clauses[i]), dst, table, ls);
        }
        if (ast->cond == NULL && ast->sval != NULL) {
            break;
        } else {
            lambdas(ast->cond, dst, table, ls);
        }
        break;
    case NODE_UQOPR:
        lambdas(ast->left, dst, table, ls);
        break;
    case NODE_QTOPR:
        lambdas(ast->left, dst, table, ls);
        break;
    case NODE_CQUOT:
         if (ast->clauses != NULL) {
            for (int i = 0; i < ast->clauses->len; i++) {
                lambdas(&(ast->clauses->clauses[i]), dst, table, ls);
            }
        }
        break;
    }
}

void lfree (node_t* ast, FILE* dst, lexicon_t* table, scope_t* scope) {
    if (__comp_trace__) { printf("lfree: node %s\n", nodespecies(ast)); }
    if (ast == NULL) return;
    switch (ast->ntype) {
    case NODE_BOOLV:
    case NODE_ELIST:
    case NODE_INTEG:
    case NODE_STRNG:
    case NODE_LQFLG:
    case NODE_LQUOT:
    case NODE_LQINT:
    case NODE_LQSTR:
        break;
    case NODE_VARBL:
        sbind(scope, ast->sval);
        if (__comp_trace__) { printf("lfree: discovered %s\n", ast->sval); }
        break;
    case NODE_IUNOP:
        lfree(ast->left, dst, table, scope);
        break;
    case NODE_IBNOP:
        lfree(ast->left, dst, table, scope);
        lfree(ast->right, dst, table, scope);
        break;
    case NODE_IFEXP:
        lfree(ast->cond, dst, table, scope);
        lfree(ast->left, dst, table, scope);
        lfree(ast->right, dst, table, scope);
        break;
    case NODE_LETEX:
        {
        scope_t* lets = scope_a();
        for (int i = 0; i < ast->clauses->len; i++) {
            lfree(ast->clauses->clauses[i].left, dst, table, scope);
            sbind(lets, ast->clauses->clauses[i].sval);
        }
        lfree(ast->left, dst, table, scope);
        unbind(scope, lets);
        scope_d(lets);
        break;
        }
        break;
    case NODE_CLOSE:
        {
        scope_t* lets = scope_a();
        for (int i = 0; i < ast->clauses->len; i++) {
            sbind(lets, ast->clauses->clauses[i].sval);
        }
        lfree(ast->left, dst, table, scope);
        unbind(scope, lets);
        scope_d(lets);
        break;
        }
        break;
    case NODE_CONDE:
        for (int i = 0; i < ast->clauses->len; i++) {
            lfree(ast->clauses->clauses[i].left, dst, table, scope);
            lfree(ast->clauses->clauses[i].right, dst, table, scope);
        }
        lfree(ast->cond, dst, table, scope);
        break;
    case NODE_CALLF:
        for (int i = 0; i < ast->clauses->len; i++) {
            lfree(&(ast->clauses->clauses[i]), dst, table, scope);
        }
        if (ast->cond == NULL && ast->sval != NULL) {
            if (get_args(table, ast->sval) == -1) {
                sbind(scope, ast->sval);
            }
        } else {
            lfree(ast->cond, dst, table, scope);
        }
        break; 
    case NODE_UQOPR:
        lfree(ast->left, dst, table, scope);
        break;
    case NODE_QTOPR:
        lfree(ast->left, dst, table, scope);
        break;
    case NODE_CQUOT:
         if (ast->clauses != NULL) {
            for (int i = 0; i < ast->clauses->len; i++) {
                lfree(&(ast->clauses->clauses[i]), dst, table, scope);
            }
        }
        break;  
    }
    if (__comp_trace__) { printf("lfree RETURNING FROM: node %s\n", nodespecies(ast)); }
}

char* nodespecies(node_t* ast) {
    if (ast == NULL) return "null";
    switch (ast->ntype) {
    case NODE_BINDE:
        return "bind_expression";
    case NODE_BOOLV:
        return "boolean";
    case NODE_CALLE:    
        return "call_depracated";
    case NODE_CALLF:
        return "call";
    case NODE_CLOSE:    
        return "lambda/closure";
    case NODE_CONDC:
        return "conditional_clause";
    case NODE_CONDE:
        return "conditional";
    case NODE_DEFFN:
        return "define";
    case NODE_ELIST:
        return "empty_list";
    case NODE_IBNOP:
        return "binary_operation";
    case NODE_IFEXP:
        return "if_expression";
    case NODE_INTEG:
        return "integer";
    case NODE_IUNOP:
        return "unary_operation";
    case NODE_LETEX:
        return "lex_expression";
    case NODE_STRNG:
        return "node_string";
    case NODE_VARBL:
        return "node_variable";
    }
    return "unknown";
}