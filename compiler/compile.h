#if !defined(COMPILE_H)
#define COMPILE_H

/* registers */
#define RAX "rax"
#define RDI "rdi"
#define RSI "rsi"
#define RCX "rcx"
#define RBX "rbx"
#define RDX "rdx"
#define RSP "rsp"
#define RBP "rbp"
#define MOV "mov"
#define R8  "r8"
#define R9  "r9"
#define R10 "r10"
#define R11 "r11"
#define R12 "r12"
#define R13 "r13"
#define R14 "r14"
#define R15 "r15"

/* builtin functions */
#define ERR "err"

#define TAG_BITS 0b110111 /* ignores garbage bit */

#define TAG_INT   0b000000
#define TAG_FLAG  0b010000
#define TAG_BYTES 0b100000
#define TAG_EMPTY 0b110000
#define TAG_ELIST 0b00110000
#define TAG_ESTRN 0b01110000
#define TAG_ESYMB 0b10110000

#define TAG_STRING         0b100
#define TAG_SYMBOL         0b101
#define TAG_COMSYM         0b110
#define TAG_BOX            0b001
#define TAG_LIST           0b010
#define TAG_PROC           0b011


#define TAG_SYMBOLQ        0b00111 /* immutable symbol */
#define TAG_SYMBOLF        0b01111 /* function pointer */
#define TAG_SYMBOLM        0b10111 /* quasisymbol (mutable) */
#define TAG_SYMBOLK        0b11111 /* lexical pointer */

#define TAG_SYMBOL_DEF 255

#define TAG_DESTROYER 0xFFFFFFFFFFFFFFF8
#define TAG_PROCD 0x8000000000000003
#define TAG_PROCD_MASKER 0x7FFFFFFFFFFFFFF8

/* err codes */
#define ERR_ASSERT_INTEGER 0x00
#define ERR_ASSERT_BOOLEAN 0x10
#define ERR_ASSERT_BYTES   0x20
#define ERR_ASSERT_ELIST   0x30
#define ERR_ASSERT_BOX     0x40
#define ERR_ASSERT_LIST    0x80
#define ERR_ASSERT_PROC    0xC0
#define ERR_HEAP_EXHAUSTED 0x90
#define ERR_HEAP_OBJECTBIG 0xA0
#define ERR_STACK_OVERFLOW 0xB0
#define ERR_DIVIDE_ZERO    0xD0

/* compiler constants */
#define COMPILER_WORDSIZE 8

/* GC CONSTANTS */
#define GC_SCOPE_CREATE  1
#define GC_SCOPE_DESTROY 2

/* program header */
#define HEADER "\tglobal entry\n\textern error\n\tsection .text\n\nentry:\n"

#include "parse.h"

typedef struct __nodelist {
    int len;
    int cap;
    node_t** ls;
} nodelist_t;

void compile (char* path);

#endif