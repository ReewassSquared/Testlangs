#if !defined(PARSE_H)
#define PARSE_H

#define NODE_UNDEF 0
#define NODE_INTEG 1
#define NODE_STRNG 2
#define NODE_BOOLV 3
#define NODE_IUNOP 4
#define NODE_IBNOP 5
#define NODE_IFEXP 6
#define NODE_CONDE 7
#define NODE_CONDC 8
#define NODE_LETEX 9
#define NODE_BINDE 10
#define NODE_VARBL 11
#define NODE_ELIST 12
#define NODE_DEFFN 14
#define NODE_CALLF 15
#define NODE_CALLE 16
#define NODE_CLOSE 17
#define NODE_LQUOT 18
#define NODE_CQUOT 19
#define NODE_LQINT 20
#define NODE_LQSTR 21
#define NODE_LQFLG 22
#define NODE_QTOPR 23
#define NODE_UQOPR 24
#define NODE_QMARK 25
#define NODE_DEFLT 26
#define NODE_MATCH 27
#define NODE_MTCHC 28
#define NODE_BEGIN 29
#define NODE_GLOBL 30
#define NODE_MUTST 31
#define NODE_MULTI 32
#define NODE_FLOPN 33
#define NODE_FLWRT 34
#define NODE_FLCLS 35

#define OP_UND 0
#define OP_ADD 1
#define OP_SUB 2
#define OP_MUL 3
#define OP_DIV 4
#define OP_ABS 5
#define OP_NEG 6
#define OP_INC 7
#define OP_DEC 8
#define OP_APP 9
#define OP_PRT 10
#define OP_CAR 11
#define OP_CDR 12
#define OP_BOX 14
#define OP_UBX 15
#define OP_CNS 16
#define CMP_GT 17
#define CMP_GE 18
#define CMP_EQ 19
#define CMP_LE 20
#define CMP_LT 21
#define CMP_NE 22
#define CMP_ZR 23
#define OP_FUN 24
#define OP_IDT 25
#define OP_AND 26
#define OP_LOR 27
#define CHKSTR 28
#define CHKLST 29
#define CHKELS 30
#define CHKINT 31
#define CHKFLG 32
#define CHKBOO 33
#define CHKSYM 34
#define CHKPRC 35
#define CHKBOX 36
#define CHKBYT 37
#define OP_LEN 38
#define CMP_SE 39
#define CMP_RE 40
#define OP_MOD 41
#define BITAND 42
#define BITLOR 43
#define BITXOR 44
#define BITSHL 45
#define BITSHR 46
#define OP_NOT 47
#define CHKCNS 48
#define OP_SAP 49

#define TYPE_U 0
#define TYPE_I 1
#define TYPE_S 2
#define TYPE_B 3

#include <stdio.h>
#include "lex.h"

typedef uint32_t ntype_t;
typedef uint8_t etype_t;
typedef uint8_t otype_t;

typedef struct __clauses {
    struct __node* clauses; 
    int len;
    int cap;
} clauses_t;

typedef struct __node {
    /* srcpos info */
    int l;
    int c;

    /* node type */
    ntype_t ntype;

    /* tree structure */
    struct __node* left;
    struct __node* right;
    struct __node* cond;
    /* will be array */
    clauses_t* clauses;

    /* values (used by primary) */
    char* sval;
    uint8_t bval;
    int64_t ival;

    /* expression type, op type, cmp type */
    etype_t etype;
    otype_t otype;

    /* used for suppressing output */
    uint8_t supress;
} node_t;

node_t* parse(tokstring_t* toks);
node_t* newnode(node_t* node);

#endif