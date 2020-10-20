#if !defined(LEX_H)
#define LEX_H

#define TOK_UNDEFN 0
#define TOK_LPAREN 1
#define TOK_RPAREN 2
#define TOK_NUMLIT 3
#define TOK_STRLIT 4
#define TOK_IDENTF 5
#define TOK_ADDOPR 6
#define TOK_SUBOPR 7
#define TOK_MULOPR 8
#define TOK_DEFINE 9
#define TOK_DIVOPR 10
#define TOK_CONDIT 11
#define TOK_ZEROIF 12
#define TOK_IFWORD 14
#define TOK_ELSEWD 15
#define TOK_ABSOPR 16
#define TOK_GTCOMP 17
#define TOK_GECOMP 18
#define TOK_EQCOMP 19
#define TOK_LECOMP 20
#define TOK_LTCOMP 21
#define TOK_INCOPR 22
#define TOK_DECOPR 23
#define TOK_NECOMP 24
#define TOK_PRTOPR 25
#define TOK_BVTRUE 26
#define TOK_BVFALS 27
#define TOK_LETLET 28
#define TOK_BOXBOX 29
#define TOK_UNBOXE 30
#define TOK_CONSLT 31
#define TOK_CARCAR 32
#define TOK_CDRCDR 33
#define TOK_EMPTYL 34
#define TOK_FNPROC 35
#define TOK_FNCALL 36
#define TOK_LAMBDA 37
#define TOK_LQUOTE 38
#define TOK_QQUOTE 39
#define TOK_PERIOD 40
#define TOK_UNQUOT 41
#define TOK_UNQTLS 42
#define TOK_SYMBOL 43
#define TOK_STREQL 44
#define TOK_CHKSTR 45
#define TOK_CHKLST 46
#define TOK_CHKELS 47
#define TOK_CHKINT 48
#define TOK_CHKFLG 49
#define TOK_CHKBOO 50
#define TOK_CHKSYM 51
#define TOK_SYMEQL 52
#define TOK_CHKPRC 53
#define TOK_CHKBOX 54
#define TOK_CHKBYT 55
#define TOK_ANDAND 56
#define TOK_OROROR 57
#define TOK_LENGTH 58
#define TOK_LISTBD 59
#define TOK_MODMOD 60
#define TOK_NOTNOT 61
#define TOK_BITAND 62
#define TOK_BITLOR 63
#define TOK_BITSHL 64
#define TOK_BITSHR 65
#define TOK_BITXOR 66
#define TOK_MATCHE 67
#define TOK_QUMARK 68
#define TOK_CHKCNS 69
#define TOK_MTCHDF 70
#define TOK_SUPRSS 71
#define TOK_MUTSET 72
#define TOK_ENDOFL 0xFFFF

#define DEFAULT_TOKSTRING_SIZE 1024


#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

typedef uint32_t tok_t;
typedef struct __lexer {
    int l;
    int c;
    tok_t tok;
    uint8_t* buf;
    int b;
    int r;
    int e;
    char* val;
    char ch;
    int sz;
} lexer_t;

typedef struct __token {
    tok_t tok;
    int l;
    int c;
    char* val;
} token_t;

typedef struct __coord {
    int l;
    int c;
} coord_t;

typedef struct __tokstring {
    int len;
    int cap;
    int off; /* used by parser */
    token_t* toks;
} tokstring_t;


int lex(lexer_t* lex, uint8_t* src, size_t sz, tokstring_t* toks);
char* tok_name (tok_t tok);

#endif