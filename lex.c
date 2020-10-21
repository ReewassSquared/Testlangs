#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "lex.h"

static volatile int __lex_trace__ = 0;

/* used for lexing quotes. Treats '.' as TOK_PERIOD */
static volatile int __lex_parendepth__ = -1;
static volatile int __lex_parendepth2__ = -1;
static volatile int __lex_exprdepth__ = -1;
static volatile int __lex_inexpr__ = 0;
static volatile int __lex_lastone__ = 0;

coord_t lex_begin (lexer_t* lex);
void lex_end (lexer_t* lex);
void lex_segment (lexer_t* lex);
void lex_nextch (lexer_t* lex);
void lex_rewind (lexer_t* lex);
void lex_skipto (lexer_t* lex, char tok);
int lex_isdigit (lexer_t* lex);
int lex_isident (lexer_t* lex);
token_t lex_next (lexer_t* lex);
void lex_number (lexer_t* lex);
void lex_ident (lexer_t* lex);
void lex_keyword (lexer_t* lex);
void lex_string (lexer_t* lex);
void lex_error (lexer_t* lex, char* msg, ...);
void tokstring_grow (tokstring_t* toks, int cap);
tokstring_t* lex_quasiquote (lexer_t* lex);
tokstring_t* lex_quote(lexer_t* lex);
tokstring_t* lex_normal(lexer_t* lex);
tokstring_t* tokstring_append (tokstring_t* toks, token_t tok);
tokstring_t* tokstring_extend (tokstring_t* toks, tokstring_t* toks2);
tokstring_t* tokstring_append_ (tokstring_t* toks, token_t tok, int prnt);

tokstring_t* tokstring_append_ (tokstring_t* toks, token_t tok, int prnt) {
    if (prnt) {
        if (__lex_trace__) {
            char* __prv = (tok.val == NULL) ? "" : tok.val;
            printf("[%04d:%04d] %12s %s\n", tok.l, tok.c, tok_name(tok.tok), __prv);
        }
    }

    if (toks == NULL) {
        toks = malloc(sizeof(tokstring_t));
        toks->len = 0;
        toks->cap = 0;
        toks->toks = NULL;
        tokstring_grow(toks, 16);
        toks->toks[toks->len++] = tok;
        return toks;
    }
    else {
        if (2 * toks->len >= toks->cap) {
            tokstring_grow(toks, 0);
        }
        toks->toks[toks->len++] = tok;
        return toks;
    }
}

tokstring_t* tokstring_append (tokstring_t* toks, token_t tok) {
    return tokstring_append_ (toks, tok, 1);
}

tokstring_t* tokstring_extend (tokstring_t* toks, tokstring_t* toks2) {
    if (toks2 == NULL) return toks;
    for (int i = 0; i < toks2->len; i++) {
        toks = tokstring_append_(toks, toks2->toks[i], 0);
    }
    return toks;
}

tokstring_t* lex_quasiquote (lexer_t* lex) {
    tokstring_t* toks = NULL;
    int __lex_parendepth2 = __lex_parendepth2__;
    __lex_parendepth2__ = 0;

    while (__lex_parendepth2__ >= 0) {
        coord_t coords;
        lex_end(lex);

        while (lex->ch != EOF && \
            (lex->ch == ' ' || lex->ch == '\t' || lex->ch == '\n' || lex->ch == '\r')) {
            lex_nextch(lex);
        }
    
        if (lex->ch == EOF) {
            lex->tok = TOK_ENDOFL;
            tokstring_append(toks, (token_t){lex->tok, 0, 0, NULL});
            return toks;
        }

        coords = lex_begin(lex);

        if (lex->ch == 0) {
            /* get us started */
            lex_nextch(lex);
        }

        /* we lex. */
        while (lex->ch != EOF && \
               lex->ch != ' ' && \
               lex->ch != '\t' && \
               lex->ch != '\n' && \
               lex->ch != '\r' && \
               lex->ch != '(' && \
               lex->ch != ')' && \
               lex->ch != '[' && \
               lex->ch != ']' && \
               lex->ch != ',' && \
               !((__lex_parendepth2__ > 0) && (lex->ch == '.'))) {
            if (lex->ch != ')' && lex->ch != ']') { __lex_lastone__ = 1; }
            lex_nextch(lex);
        }
    
        switch (lex->ch) {
        case '(':
        case '[':
            __lex_parendepth2__++;
            lex_nextch(lex);
            lex->tok = TOK_LPAREN;
            toks = tokstring_append(toks, (token_t){lex->tok, coords.l, coords.c, lex->val});
            break;
        case ')':
        case ']':
            if (__lex_lastone__ && lex->buf[lex->r - 2] != '(' && lex->buf[lex->r - 2] != '[' \
                && lex->buf[lex->r - 2] != ' ' && lex->buf[lex->r - 2] != '\t' && lex->buf[lex->r - 2] != '\n' && lex->buf[lex->r - 2] != '\r') {
                /* a symbol precludes this */
                lex_segment(lex);
                lex->tok = TOK_SYMBOL;
                toks = tokstring_append(toks, (token_t){lex->tok, coords.l, coords.c, lex->val});
                lex->val = NULL;
            }
            lex_nextch(lex);
            lex->tok = TOK_RPAREN;
            toks = tokstring_append(toks, (token_t){lex->tok, coords.l, coords.c, NULL});
            __lex_parendepth2__--;
            if (__lex_parendepth2__ <= 0) {
                __lex_parendepth2__ = __lex_parendepth2;
                return toks;
            }
            break;
        case '.':
            if (__lex_parendepth2__ > 0) {
                lex_nextch(lex);
                lex->tok = TOK_PERIOD;
                toks = tokstring_append(toks, (token_t){lex->tok, coords.l, coords.c, NULL});
                break;
            }
        case ',':
            lex_nextch(lex);
            if (lex->ch == '@') {
                lex_nextch(lex);
                lex->tok = TOK_UNQTLS;
                toks = tokstring_append(toks, (token_t){lex->tok, coords.l, coords.c, NULL});
            }
            else {
                lex->tok = TOK_UNQUOT;
                toks = tokstring_append(toks, (token_t){lex->tok, coords.l, coords.c, NULL});
            }
            toks = tokstring_extend(toks, lex_normal(lex));
            if (__lex_parendepth2__ <= 0) {
                __lex_parendepth2__ = __lex_parendepth2;
                return toks;
            }
            break;
        default:
            lex->tok = TOK_SYMBOL;
            lex_segment(lex);
            toks = tokstring_append(toks, (token_t){lex->tok, coords.l, coords.c, lex->val});
            lex_nextch(lex);
            if (__lex_parendepth2__ <= 0) {
                __lex_parendepth2__ = __lex_parendepth2;
                return toks;
            }
        }

        __lex_lastone__ = 0;
    }
}

int lex (lexer_t* lex, uint8_t *src, size_t sz, tokstring_t* toks) {
    int ntoks = 0;
    token_t tok;

    /* initialize lexer */
    lex->buf = src;
    lex->l = 1;
    lex->c = 1;
    lex->tok = TOK_UNDEFN;
    lex->b = -1;
    lex->r = 0;
    lex->e = 0;
    lex->val = NULL;
    lex->sz = (int) sz;
    lex->ch = 0;

    /* initialize token string */
    toks->len = 0;
    tokstring_grow(toks, DEFAULT_TOKSTRING_SIZE);

    while (lex->tok != TOK_ENDOFL) {
        toks = tokstring_extend(toks, lex_normal(lex));
    }

    /* cleanup */
    free(lex->buf);

    //for (int i = 0; i < toks->len; i++) {
        
    //}

    return toks->len;
}

coord_t lex_begin (lexer_t* lex) { 
    lex->b = lex->r - 1; 
    lex->val = NULL;
    return (coord_t){lex->l, lex->c - 1};
}

void lex_end (lexer_t* lex) { 
    lex->b = -1; 
    lex->val = NULL;
}

void lex_segment (lexer_t* lex) { 
    free(lex->val);
    lex->val = malloc(lex->r - lex->b); 
    memcpy(lex->val, &(lex->buf[lex->b]), lex->r - lex->b - 1);
    lex->val[lex->r - lex->b - 1] = 0;
    if (__lex_trace__) { printf("segment: %s\n", lex->val); }
}

void lex_nextch (lexer_t* lex) {
    if (lex->e >= lex->sz) {
        lex->ch = EOF;
        return;
    } else {
        lex->ch = (char) lex->buf[lex->r++];
        //printf("%c", lex->ch);
        if (lex->ch == '\n') {
            lex->l++;
            lex->c = 1;
        }
        else {
            lex->c++;
        }
        lex->e++;
    }
}

void lex_rewind (lexer_t* lex) {
    lex->ch = (char) lex->buf[--lex->r];
    lex->e--;
}

void lex_skipto (lexer_t* lex, char tok) {
    while (lex->ch != tok && lex->ch != EOF) {
        lex_nextch(lex);
    }
    lex_nextch(lex);
}

int lex_isdigit (lexer_t* lex) {
    return (int) (lex->ch >= '0' && lex->ch <= '9');
}

int lex_isident (lexer_t* lex) {
    return (int) ((lex->ch >= 'A' && lex->ch <= 'Z') \
               || (lex->ch >= 'a' && lex->ch <= 'z') \
               || lex->ch == '_');
}

tokstring_t* lex_quote (lexer_t* lex) {
    if (__lex_trace__) printf("ENTERING LEX_QUOTE...\n");
    tokstring_t* toks = NULL;

    int __lex_parendepth = __lex_parendepth__;
    __lex_parendepth__ = 0;

    while (__lex_parendepth__ >= 0) {
        coord_t coords;

        lex_end(lex);

        while (lex->ch != EOF && \
            (lex->ch == ' ' || lex->ch == '\t' || lex->ch == '\n' || lex->ch == '\r')) {
            lex_nextch(lex);
        }
    
        if (lex->ch == EOF) {
            lex->tok = TOK_ENDOFL;
            return tokstring_append(toks, (token_t){lex->tok, 0, 0, NULL});
        }

        coords = lex_begin(lex);

        if (lex->ch == 0) {
            /* get us started */
            lex_nextch(lex);
        }

        lex->val == NULL;

        while (lex->ch != EOF && \
               lex->ch != ' ' && \
               lex->ch != '\t' && \
               lex->ch != '\n' && \
               lex->ch != '\r' && \
               lex->ch != '(' && \
               lex->ch != ')' && \
               lex->ch != '[' && \
               lex->ch != ']' && \
               !((__lex_parendepth__ > 0) && (lex->ch == '.'))) {
            __lex_lastone__ = 1;
            lex_nextch(lex);
        }
        
        switch (lex->ch) {
        case '(':
        case '[':
            lex_nextch(lex);
            lex->tok = TOK_LPAREN;
            toks = tokstring_append(toks, (token_t){lex->tok, coords.l, coords.c, NULL});
            __lex_parendepth__++;
            break;
        case ')':
        case ']':
            if (__lex_lastone__ && lex->buf[lex->r - 2] != '(' && lex->buf[lex->r - 2] != '[' \
               && lex->buf[lex->r - 2] != ' ' && lex->buf[lex->r - 2] != '\t' && lex->buf[lex->r - 2] != '\n' && lex->buf[lex->r - 2] != '\r') {
                /* a symbol precludes this */
                lex_segment(lex);
                lex->tok = TOK_SYMBOL;
                toks = tokstring_append(toks, (token_t){lex->tok, coords.l, coords.c, lex->val});
            }
            lex_nextch(lex);
            lex->tok = TOK_RPAREN;
            toks = tokstring_append(toks, (token_t){lex->tok, coords.l, coords.c, NULL});
            __lex_parendepth__--;
            if (__lex_parendepth__ <= 0) {
                __lex_parendepth__ = __lex_parendepth;
                return toks;
            }
            break;
        case '.':
            if (__lex_parendepth__ > 0) {
                lex_nextch(lex);
                lex->tok = TOK_PERIOD;
                toks = tokstring_append(toks, (token_t){lex->tok, coords.l, coords.c, NULL});
                break;
            }
        default:
            /* basically we make it into literal */
            lex->tok = TOK_SYMBOL;
            lex_segment(lex);
            toks = tokstring_append(toks, (token_t){lex->tok, coords.l, coords.c, lex->val});
            lex_nextch(lex);
            if (__lex_parendepth__ <= 0) {
                __lex_parendepth__ = __lex_parendepth;
                return toks;
            }
        }
        
        __lex_lastone__ = 0;
    }
}

tokstring_t* lex_normal (lexer_t* lex) {
    tokstring_t* toks = NULL;

    int __lex_exprdepth = __lex_exprdepth__;
    __lex_exprdepth__ = 0;

    while (__lex_exprdepth__ >= 0) {
        token_t tok = lex_next(lex);
        switch (tok.tok) {
        case TOK_STRLIT:
        case TOK_NUMLIT:
        case TOK_BVTRUE:
        case TOK_BVFALS:
        case TOK_IDENTF:
            toks = tokstring_append(toks, tok);
            if (__lex_exprdepth__ <= 0) {
                __lex_exprdepth__ = __lex_exprdepth;
                return toks;
            }
            break;
        case TOK_LPAREN:
            toks = tokstring_append(toks, tok);
            __lex_exprdepth__++;
            break;
        case TOK_RPAREN:
            toks = tokstring_append(toks, tok);
            __lex_exprdepth__--;
            if (__lex_exprdepth__ <= 0) {
                __lex_exprdepth__ = __lex_exprdepth;
                return toks;
            }
            break;
        case TOK_QQUOTE:
            toks = tokstring_append(toks, tok);
            toks = tokstring_extend(toks, lex_quasiquote(lex));
            if (__lex_exprdepth__ <= 0) {
                __lex_exprdepth__ = __lex_exprdepth;
                return toks;
            }
            break;
        case TOK_LQUOTE:
            toks = tokstring_append(toks, tok);
            toks = tokstring_extend(toks, lex_quote(lex));
            if (__lex_exprdepth__ <= 0) {
                __lex_exprdepth__ = __lex_exprdepth;
                return toks;
            }
            break;
        case TOK_ENDOFL:
            toks = tokstring_append(toks, tok);
            __lex_exprdepth__--;
            __lex_exprdepth__ = __lex_exprdepth;
            return toks;
        default:
            toks = tokstring_append(toks, tok);
        }
    }
    return toks;
}

token_t lex_next (lexer_t* lex) {
    coord_t coords;
redo:
    lex_end(lex);

    while (lex->ch != EOF && \
        (lex->ch == ' ' || lex->ch == '\t' || lex->ch == '\n' || lex->ch == '\r')) {
        lex_nextch(lex);
    }
    
    if (lex->ch == EOF) {
        lex->tok = TOK_ENDOFL;
        return (token_t){lex->tok, 0, 0, NULL};
    }

    coords = lex_begin(lex);

    if (lex->ch == 0) {
        /* get us started */
        lex_nextch(lex);
    }

    /* clean val */
    lex->val = NULL;
    
    switch (lex->ch) {
    case '(':
    case '[':
        {
            char tch = lex->ch;
            lex_nextch(lex);
            if (lex->ch == ')' && tch == '(') {
                lex_nextch(lex);
                lex->tok = TOK_EMPTYL;
                break;
            }
        }
        lex->tok = TOK_LPAREN;
        break;
    case ')':
    case ']':
        lex_nextch(lex);
        lex->tok = TOK_RPAREN;
        break;
    case '+':
        lex_nextch(lex);
        lex->tok = TOK_ADDOPR;
        break;
    case '-':
        lex_nextch(lex);
        if (lex_isdigit(lex)) {
            lex_number(lex);
            break;
        }
        lex->tok = TOK_SUBOPR;
        break;
    case '*':  
        lex_nextch(lex);
        lex->tok = TOK_MULOPR;
        break;
    case ',':
        lex_nextch(lex);
        lex->tok = TOK_UNQUOT;
        break;
    case '/':
        lex_nextch(lex);
        lex->tok = TOK_DIVOPR;
        break;
    case ';':
        lex_nextch(lex);
        if (lex->ch == ';') {
            lex_nextch(lex);
            lex_skipto(lex, '\n');
            goto redo;
        }
        lex_skipto(lex, '\n');
        goto redo;
    case '\"':
        lex_nextch(lex);
        lex_begin(lex);
        lex_string(lex);
        lex->tok = TOK_STRLIT;
        break;
    case '>':
        lex_nextch(lex);
        if (lex->ch == '=') {
            lex_nextch(lex);
            lex->tok = TOK_GECOMP;
            break;
        }
        lex->tok = TOK_GTCOMP;
        break;
    case '<':
        lex_nextch(lex);
        if (lex->ch == '=') {
            lex_nextch(lex);
            lex->tok = TOK_LECOMP;
            break;
        }
        lex->tok = TOK_LTCOMP;
        break;
    case '=':
        lex_nextch(lex);
        lex->tok = TOK_EQCOMP;
        break;
    case '~':
        lex_nextch(lex);
        lex->tok = TOK_NECOMP;
        break;
    case '\'':
        lex_nextch(lex);
        lex->tok = TOK_LQUOTE;
        break;
    case '`':
        lex_nextch(lex);
        lex->tok = TOK_QQUOTE;
        break;
    case '?':
        lex_nextch(lex);
        if (lex->ch == ' ' || lex->ch == '\t' || lex->ch == '\n' || lex->ch == '\r') {
            lex->tok = TOK_QUMARK;
            break;
        }
    case '#':
        lex_nextch(lex);
        if (lex->ch == 't') {
            lex_nextch(lex);
            lex->tok = TOK_BVTRUE;
            break;
        }
        else if (lex->ch == 'f') {
            lex_nextch(lex);
            lex->tok = TOK_BVFALS;
            break;
        }
        else if (lex->ch == ';') {
            lex_nextch(lex);
            lex->tok = TOK_SUPRSS;
            break;
        }
        else {
            lex_skipto(lex, ' ');
            lex_rewind(lex);
            lex_segment(lex);
            lex_error(lex, "unknown value after octothorp: %s", (char*) lex->val);
            free(lex->val);
            goto redo;
        }
        break;
    default:
        if (lex_isdigit(lex)) {
            lex_number(lex);
        }
        else if (lex_isident(lex)) {
            lex_nextch(lex);
            lex_ident(lex);
        } else {
            lex_skipto(lex, ' ');
            lex->tok = TOK_UNDEFN;
            lex_error(lex, "undefined token");
        }
    }

    /* we return a token_t */
    return (token_t){lex->tok, coords.l, coords.c, lex->val};
}

void lex_number (lexer_t* lex) {
    while (lex->ch != EOF && lex_isdigit(lex)) {
        lex_nextch(lex);
    }
    lex_segment(lex);
    lex->tok = TOK_NUMLIT;
}

void lex_ident (lexer_t* lex) {
    while (lex->ch != EOF \
        && lex->ch != ' ' \
        && lex->ch != '\t' \
        && lex->ch != '\r' \
        && lex->ch != '\n' \
        && lex->ch != '(' \
        && lex->ch != ')' \
        && lex->ch != '[' \
        && lex->ch != ']' \
        && lex->ch != ';' \
        && lex->ch != '\"') {
        lex_nextch(lex);
    }
    lex_segment(lex);
    lex_keyword(lex);
}

void lex_keyword (lexer_t* lex) {
    if (!strcmp(lex->val, "cond")) {
        lex->tok = TOK_CONDIT;
    }
    else if (!strcmp(lex->val, "abs")) {
        lex->tok = TOK_ABSOPR;
    }
    else if (!strcmp(lex->val, "if")) {
        lex->tok = TOK_IFWORD;
    }
    else if (!strcmp(lex->val, "else")) {
        lex->tok = TOK_ELSEWD;
    }
    else if (!strcmp(lex->val, "zero?")) {
        lex->tok = TOK_ZEROIF;
    }
    else if (!strcmp(lex->val, "add1")) {
        lex->tok = TOK_INCOPR;
    }
    else if (!strcmp(lex->val, "sub1")) {
        lex->tok = TOK_DECOPR;
    }
    else if (!strcmp(lex->val, "print")) {
        lex->tok = TOK_PRTOPR;
    }
    else if (!strcmp(lex->val, "let")) {
        lex->tok = TOK_LETLET;
    }
    else if (!strcmp(lex->val, "box")) {
        lex->tok = TOK_BOXBOX;
    }
    else if (!strcmp(lex->val, "unbox")) {
        lex->tok = TOK_UNBOXE;
    }
    else if (!strcmp(lex->val, "cons")) {
        lex->tok = TOK_CONSLT;
    }
    else if (!strcmp(lex->val, "car")) {
        lex->tok = TOK_CARCAR;
    }
    else if (!strcmp(lex->val, "cdr")) {
        lex->tok = TOK_CDRCDR;
    }
    else if (!strcmp(lex->val, "define")) {
        lex->tok = TOK_DEFINE;
    }
    else if (!strcmp(lex->val, "fun")) {
        lex->tok = TOK_FNPROC;
    }
    else if (!strcmp(lex->val, "call")) {
        lex->tok = TOK_FNCALL;
    }
    else if (!strcmp(lex->val, "lambda")) {
        lex->tok = TOK_LAMBDA;
    }
    else if (!strcmp(lex->val, "string?")) {
        lex->tok = TOK_CHKSTR;
    }
    else if (!strcmp(lex->val, "integer?")) {
        lex->tok = TOK_CHKINT;
    }
    else if (!strcmp(lex->val, "flag?")) {
        lex->tok = TOK_CHKFLG;
    }
    else if (!strcmp(lex->val, "boolean?")) {
        lex->tok = TOK_CHKBOO;
    }
    else if (!strcmp(lex->val, "list?")) {
        lex->tok = TOK_CHKLST;
    }
    else if (!strcmp(lex->val, "emptylist?")) {
        lex->tok = TOK_CHKELS;
    }
    else if (!strcmp(lex->val, "box?")) {
        lex->tok = TOK_CHKBOX;
    }
    else if (!strcmp(lex->val, "proc?")) {
        lex->tok = TOK_CHKPRC;
    }
    else if (!strcmp(lex->val, "bytes?")) {
        lex->tok = TOK_CHKBYT;
    }
    else if (!strcmp(lex->val, "symbol?")) {
        lex->tok = TOK_CHKSYM;
    }
    else if (!strcmp(lex->val, "string=?")) {
        lex->tok = TOK_STREQL;
    }
    else if (!strcmp(lex->val, "symbol=?")) {
        lex->tok = TOK_SYMEQL;
    }
    else if (!strcmp(lex->val, "and")) {
        lex->tok = TOK_ANDAND;
    }
    else if (!strcmp(lex->val, "or")) {
        lex->tok = TOK_OROROR;
    }
    else if (!strcmp(lex->val, "length")) {
        lex->tok = TOK_LENGTH;
    }
    else if (!strcmp(lex->val, "list")) {
        lex->tok = TOK_LISTBD;
    }
    else if (!strcmp(lex->val, "mod")) {
        lex->tok = TOK_MODMOD;
    }
    else if (!strcmp(lex->val, "bitand")) {
        lex->tok = TOK_BITAND;
    }
    else if (!strcmp(lex->val, "bitor")) {
        lex->tok = TOK_BITLOR;
    }
    else if (!strcmp(lex->val, "bitxor")) {
        lex->tok = TOK_BITXOR;
    }
    else if (!strcmp(lex->val, "bitshl")) {
        lex->tok = TOK_BITSHL;
    }
    else if (!strcmp(lex->val, "bitshr")) {
        lex->tok = TOK_BITSHR;
    }
    else if (!strcmp(lex->val, "not")) {
        lex->tok = TOK_NOTNOT;
    }
    else if (!strcmp(lex->val, "match")) {
        lex->tok = TOK_MATCHE;
    }
    else if (!strcmp(lex->val, "cons?")) {
        lex->tok = TOK_CHKCNS;
    }
    else if (!strcmp(lex->val, "_")) {
        lex->tok = TOK_MTCHDF;
    }
    else if (!strcmp(lex->val, "set!")) {
        lex->tok = TOK_MUTSET;
    }
    else if (!strcmp(lex->val, "file-open")) {
        lex->tok = TOK_FLOPEN;
    }
    else if (!strcmp(lex->val, "file-write")) {
        lex->tok = TOK_FLWRIT;
    }
    else if (!strcmp(lex->val, "file-close")) {
        lex->tok = TOK_FLCLOS;
    }
    else if (!strcmp(lex->val, "symbol-append")) {
        lex->tok = TOK_SYMAPP;
    }
    else {
        lex->tok = TOK_IDENTF;
        return;
    }
    free(lex->val);
    lex->val = NULL;
}

/* the val buffer is set manually */
void lex_string(lexer_t* lex) {
    char* buf = NULL;
    int i = 0;
    int vlen;
    int cptz = 0;
    int maxsz = 1; /* terminator */
    while (lex->ch != EOF) {
        maxsz++;
        if (lex->ch == '\"') {
            lex_segment(lex);
            lex_nextch(lex);
            break;
        }
        else if (lex->ch == '\\') {
            lex_nextch(lex);
            lex_nextch(lex);
        }
        else {
            lex_nextch(lex);
        }
    }
    /* valid escape suffix: n, r, t, \, ", ' */
    if (lex->ch == EOF) {
        lex_error(lex, "unterminated string");
        lex_segment(lex);
    }

    buf = malloc(maxsz);
    vlen = strlen(lex->val);

    for (; i < vlen && cptz < maxsz; i++) {
        if (lex->val[i] == '\\' && i < (vlen - 1)) {
            i++;
            switch (lex->val[i]) {
            case 'n':
                buf[cptz++] = '\n';
                break;
            case 't':
                buf[cptz++] = '\t';
                break;
            case '\r':
                buf[cptz++] = '\r';
                break;
            case '\\':
                buf[cptz++] = '\\';
                break;
            case '\"':
                buf[cptz++] = '\"';
                break;
            case '\'':
                buf[cptz++] = '\'';
                break;
            default:
                lex_error(lex, "invalid escape sequence: \\%c", (char) lex->val[i]);
                buf[cptz++] = '$';
            }
        }
        else {
            buf[cptz++] = lex->val[i];
        }
    }

    if (maxsz <= cptz) {
        lex_error(lex, "invalid string lexer implementation! maxsz: %d, bufcnt: %d", maxsz, cptz);
    }
    memset((uint8_t*) ((uintptr_t) buf + ((maxsz > cptz) ? cptz : (maxsz - 1))), 0, (maxsz > cptz) ? (maxsz - cptz) : 1);

    free(lex->val);
    lex->val = buf;
}

void lex_error (lexer_t* lex, char* msg, ...) {
    va_list v;
    va_start(v, msg);
    printf("%d:%d: ", lex->l, lex->c);
    vprintf(msg, v);
    va_end(v);
    printf("\n");
    //sleep(1);
    exit(1);
}

void tokstring_grow (tokstring_t* toks, int cap) {
    token_t* ntoks;
    if (toks->len == 0) {
        /* new tokstring */
        toks->cap = cap;
        toks->toks = malloc(sizeof(token_t) * cap);
    }
    else {
        cap = 2 * toks->cap;
        ntoks = malloc(sizeof(token_t) * cap);
        memcpy((uint8_t*) ntoks, (uint8_t*) toks->toks, sizeof(token_t) * toks->len);
        free(toks->toks);
        toks->toks = ntoks;
        toks->cap = cap;
    }
}

char* tok_name (tok_t tok) {
    switch (tok) {
    case TOK_UNDEFN:
        return "undefn";
    case TOK_LPAREN:
        return "lparen";
    case TOK_RPAREN:
        return "rparen";
    case TOK_NUMLIT:
        return "number";
    case TOK_STRLIT:
        return "string";
    case TOK_IDENTF:
        return "identifier";
    case TOK_ADDOPR:
        return "+";
    case TOK_SUBOPR:
        return "-";
    case TOK_MULOPR:
        return "*";
    case TOK_DEFINE:
        return "define";
    case TOK_DIVOPR:
        return "/";
    case TOK_CONDIT:
        return "cond";
    case TOK_ZEROIF:
        return "zero?";
    case TOK_IFWORD:
        return "if";
    case TOK_ELSEWD:
        return "else";
    case TOK_EQCOMP:
        return "=";
    case TOK_ABSOPR:
        return "abs";
    case TOK_GTCOMP:
        return ">";
    case TOK_GECOMP:
        return ">=";
    case TOK_LECOMP:
        return "<=";
    case TOK_LTCOMP:
        return "<";
    case TOK_NECOMP:
        return "~";
    case TOK_INCOPR:
        return "add1";
    case TOK_DECOPR:
        return "sub1";
    case TOK_PRTOPR:
        return "print";
    case TOK_BVTRUE:
        return "#t";
    case TOK_BVFALS:
        return "#f";
    case TOK_ENDOFL:
        return "eof";
    case TOK_LETLET:
        return "let";
    case TOK_BOXBOX:
        return "box";
    case TOK_UNBOXE:
        return "unbox";
    case TOK_CONSLT:
        return "cons";
    case TOK_CARCAR:
        return "car";
    case TOK_CDRCDR:
        return "cdr";
    case TOK_EMPTYL:
        return "()";
    case TOK_FNPROC:
        return "fun";
    case TOK_FNCALL:
        return "call";
    case TOK_LAMBDA:
        return "lambda";
    case TOK_LQUOTE:
        return "\'";
    case TOK_QQUOTE:
        return "`";
    case TOK_PERIOD:
        return ".";
    case TOK_UNQUOT:
        return ",";
    case TOK_UNQTLS:
        return ",@";
    case TOK_SYMBOL:
        return "symbol";
    case TOK_FLOPEN:
        return "file-open";
    case TOK_FLWRIT:
        return "file-write";
    case TOK_FLCLOS:
        return "file-close";
    default:
        return "unk";
    }
}
