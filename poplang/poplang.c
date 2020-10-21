#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <string.h>
#include <errno.h>

#define TAG_INT   0b000000
#define TAG_FLAG  0b010000
#define TAG_BYTES 0b100000
#define TAG_EMPTY 0b110000
#define TAG_ELIST 0b00110000
#define TAG_ESTRN 0b01110000
#define TAG_ESYMB 0b10110000

#define TAG_STRING         0b100
#define TAG_BOX            0b001
#define TAG_LIST           0b010
#define TAG_SYMBOL         0b101
#define TAG_COMSYM         0b110
#define TAG_PROC 3

#define TAG_SYMBOLQ        0b00111 /* immutable symbol */
#define TAG_SYMBOLF        0b01111 /* function pointer */
#define TAG_SYMBOLM        0b10111 /* quasisymbol (mutable) */
#define TAG_SYMBOLK        0b11111 /* lexical pointer */

#define typeof_mask  0b110111 /* ignore garbage bit */
#define val_shift    6
#define type_fixnum  TAG_INT
#define type_bool    TAG_FLAG
#define FLAG_TRUE    ((1 << val_shift) | TAG_FLAG)
#define FLAG_FALSE   TAG_FLAG

// in bytes
#define heap_size (1024 * 1024)
#define root_set_stack_Size (1024 * 1024)

static volatile int __inside_comsym__ = 0;

int64_t entry(void *);
void print_result(int64_t);
int64_t print_result_(int64_t);
void print_pair(int64_t);
void print_immediate(int64_t);
void print_special(uint64_t a);
void* __heap__;
void* __root__;
uint64_t __heapsize__ = heap_size;
uint64_t __rootsize__ = root_set_stack_Size;
volatile uint64_t __gcstack__;
volatile uint64_t __gcframe__;
FILE* __CURFILE__;
int64_t bytes_equal(int64_t a, int64_t b);
int64_t file_open_(int64_t a);
int64_t file_write_(int64_t a);
int64_t file_close_();
int64_t fprint_immediate(int64_t a);
int64_t fprint_result(int64_t a);
int64_t fprint_pair(int64_t a);
int64_t symbol_append(int64_t a, int64_t b);

int64_t file_open_(int64_t a) {
    if (__CURFILE__ != NULL) {
        printf("runtime error: tried to open a file without closing previous file.\n");
        //exit(1);
        return FLAG_FALSE;
    }

    a = *((int64_t*) (a ^ TAG_STRING));
    uint64_t car;
    int idx = 0;
    int sz_ = 64;
    char* buf = calloc(sz_, sizeof(char));
    while (a != TAG_ELIST) {
        car = *((uint64_t*) (a ^ TAG_LIST));
        for (int i = 1; i < 8; i++) {
            char __tmp = ((car >> (i*8)) & 0xFF);
            if (__tmp != 0) {
                if (4 * idx > 3 * sz_ ) {
                    sz_ *= 2;
                    char* __buf = calloc(sz_, sizeof(char));
                    memcpy(__buf, buf, idx);
                    free(buf);
                    buf = __buf;
                }
                buf[idx++] = __tmp;
            }
        }
        a = *((uint64_t*) ((a ^ TAG_LIST) + 8));
    }
    
    __CURFILE__ = fopen(buf, "w+");

    if (__CURFILE__ == NULL) {
        printf("runtime error: internal error: %s\n", strerror(errno));
        return FLAG_FALSE;
    }
    
    return FLAG_TRUE;
}

int64_t file_write_(int64_t a) {
    if (__CURFILE__ == NULL) {
        printf("runtime error: tried to write to a file without opening first.\n");
        //exit(1);
        return FLAG_FALSE;
    }
    return fprint_result(a);
}

int64_t fprint_result(int64_t a) {
    int64_t rv = FLAG_TRUE;
    switch (0b111 & a) {
    case TAG_INT:
        rv = fprint_immediate(a);
        break;
    case TAG_BOX:
        rv = fprint_result(* ((int64_t*) (a ^ TAG_BOX)));
        break;
    case TAG_LIST:
        fprintf(__CURFILE__, "(");
        rv = fprint_pair(a);
        fprintf(__CURFILE__, ")");
        break;
    case TAG_PROC:
        //printf("<proc>");
        //break;
        {
            uint64_t car = (*((uint64_t*) (a ^ TAG_PROC))) ^ 0x000000000000003;
            int64_t cdr = *((int64_t*) ((a ^ TAG_PROC) + 8));
            fprintf(__CURFILE__, "<procedure::$%lx", car);
            if (cdr == TAG_ELIST) {
            } else if ((0b111 & cdr) == TAG_LIST) {
                fprintf(__CURFILE__, " env::{");
                rv = fprint_pair(cdr);
                fprintf(__CURFILE__, "}");
            }
            fprintf(__CURFILE__, ">");
        }
        break;
    case TAG_STRING:
        {
            a = *((int64_t*) (a ^ TAG_STRING));
            uint64_t car;
            while (a != TAG_ELIST) {
                car = *((uint64_t*) (a ^ TAG_LIST));
                for (int i = 1; i < 8; i++) {
                    char __tmp = ((car >> (i*8)) & 0xFF);
                    if (__tmp != 0) {
                        fprintf(__CURFILE__, "%c", __tmp);
                    }
                }
                a = *((uint64_t*) ((a ^ TAG_LIST) + 8));
            }
        }
        break;
    case TAG_SYMBOL:
        {
            int __tag = ((a >> 8) & 0xFF);
            a = *((int64_t*) (a >> 16));
            if (__tag == 0xFF) {
                uint64_t car;
                while (a != TAG_ELIST) {
                    car = *((uint64_t*) (a ^ TAG_LIST));
                    for (int i = 1; i < 8; i++) {
                        char __tmp = ((car >> (i*8)) & 0xFF);
                        if (__tmp != 0) {
                            fprintf(__CURFILE__, "%c", __tmp);
                        }
                    }
                    a = *((uint64_t*) ((a ^ TAG_LIST) + 8));
                }
            }
            else {
                rv = fprint_result(a);
            }
        }
        break;
    case TAG_COMSYM:
        {
            fprintf(__CURFILE__, "(");
            a = *((int64_t*) (a ^ TAG_COMSYM));
            rv = fprint_pair(a);
            fprintf(__CURFILE__, ")");
        }
        break;
    default:
        rv = FLAG_FALSE;
    }
    return rv;
}

int64_t fprint_immediate(int64_t a) {
    switch(typeof_mask & a) {
    case TAG_INT:
        fprintf(__CURFILE__, "%" PRId64, a >> val_shift);
        break;
    case TAG_FLAG :
        if (a & (~typeof_mask)) {
            fprintf(__CURFILE__, "#t");
        } else {
            fprintf(__CURFILE__, "#f");
        }
        break;
    case TAG_EMPTY: /* doesn't print anything */
        break;
    default:
        return FLAG_FALSE; 
    }
    return FLAG_TRUE;
}

int64_t fprint_pair(int64_t a) {
    int64_t rv = FLAG_TRUE;
    if (a == TAG_ELIST) {
        return FLAG_TRUE;
    }
    int64_t car = * ((int64_t*) (a ^ TAG_LIST));
    int64_t cdr = * ((int64_t*) ((a + 8) ^ TAG_LIST));
    rv = fprint_result(car);
    if (rv == FLAG_FALSE) return rv;
    if (cdr == TAG_ELIST) {

    } else if ((0b111 & cdr) == TAG_LIST) {
        fprintf(__CURFILE__, " ");
        rv = fprint_pair(cdr);
        if (rv == FLAG_FALSE) return rv;
    } else {
        fprintf(__CURFILE__, " . ");
        rv = fprint_result(cdr);
        if (rv == FLAG_FALSE) return rv;
    }
    return rv;
}

int64_t file_close_(int64_t a) {
    if (__CURFILE__ == NULL) {
        printf("runtime error: tried to write to a file without opening first.\n");
        //exit(1);
        return FLAG_FALSE;
    }
    fclose(__CURFILE__);
    __CURFILE__ = NULL;
    return FLAG_TRUE;
}

int64_t bytes_equal(int64_t a, int64_t b) {
    a = *((int64_t*) a);
    b = *((int64_t*) b);
    while (a != TAG_ELIST && b != TAG_ELIST) {
        int64_t av = *((int64_t*) (a ^ TAG_LIST));
        int64_t bv = *((int64_t*) (b ^ TAG_LIST));
        if (av != bv) {
            return FLAG_FALSE;
        }
        a = *((int64_t*) ((a ^ TAG_LIST) + 8));
        b = *((int64_t*) ((b ^ TAG_LIST) + 8));
    }
    if (a != b) {
        return FLAG_FALSE;
    }
    return FLAG_TRUE;
}

int main(int argc, char** argv) {
  __heap__ = calloc(__heapsize__, 1);
  //__root__ = calloc(__rootsize__, 1);
  //__gcstack__ = (uint64_t) __root__;
  //__gcstack__ += __rootsize__;
  //__gcstack__ -= 8;
  __gcframe__ = 0;
  int64_t result = entry(__heap__);  
  free(__heap__);
  return 0;
}

void error(uint64_t c, uint64_t a) {
  printf("err $%lu (%%%lx)\n", a, a);
  printf("code $%lu (%%%lx)\n", c, c);
  exit(1);
}

void internal_error() {
  printf("\nrts-error");
  exit(1);
}

int64_t print_result_(int64_t a) {
    //print_special(a);
    print_result(a);
    printf("\n");
    return FLAG_TRUE;
}

void print_special(uint64_t a) {
    printf("err $%lu (%%%lx)\n", a, a);
  //printf("code $%lu (%%%lx)\n", c, c);
    print_result_((int64_t) a);
}

void print_result(int64_t a) {
    switch (0b111 & a) {
    case TAG_INT:
        print_immediate(a);
        break;
    case TAG_BOX:
        printf("#&");
        print_result(* ((int64_t*) (a ^ TAG_BOX)));
        break;
    case TAG_LIST:
        printf("(");
        print_pair(a);
        printf(")");
        break;
    case TAG_PROC:
        //printf("<proc>");
        //break;
        {
            uint64_t car = (*((uint64_t*) (a ^ TAG_PROC))) ^ 0x000000000000003;
            int64_t cdr = *((int64_t*) ((a ^ TAG_PROC) + 8));
            printf("<procedure::$%lx", car);
            if (cdr == TAG_ELIST) {
            } else if ((0b111 & cdr) == TAG_LIST) {
                printf(" env::{");
                print_pair(cdr);
                printf("}");
            }
            printf(">");
        }
        break;
    case TAG_STRING:
        printf("\"");
        {
            a = *((int64_t*) (a ^ TAG_STRING));
            uint64_t car;
            while (a != TAG_ELIST) {
                car = *((uint64_t*) (a ^ TAG_LIST));
                for (int i = 1; i < 8; i++) {
                    char __tmp = ((car >> (i*8)) & 0xFF);
                    if (__tmp != 0) {
                        printf("%c", __tmp);
                    }
                }
                a = *((uint64_t*) ((a ^ TAG_LIST) + 8));
            }
        }
        printf("\"");
        break;
    case TAG_SYMBOL:
        if (!__inside_comsym__) printf("\'");
        {
            int __tag = ((a >> 8) & 0xFF);
            a = *((int64_t*) (a >> 16));
            if (__tag == 0xFF) {
                uint64_t car;
                while (a != TAG_ELIST) {
                    car = *((uint64_t*) (a ^ TAG_LIST));
                    for (int i = 1; i < 8; i++) {
                        char __tmp = ((car >> (i*8)) & 0xFF);
                        if (__tmp != 0) {
                            printf("%c", __tmp);
                        }
                    }
                    a = *((uint64_t*) ((a ^ TAG_LIST) + 8));
                }
            }
            else {
                int __inside_comsym = __inside_comsym__;
                __inside_comsym__ = 1;
                print_result(a);
                __inside_comsym__ = __inside_comsym;
            }
        }
        break;
    case TAG_COMSYM:
        if (!__inside_comsym__) printf("\'");
        {
            printf("(");
            int __inside_comsym = __inside_comsym__;
            __inside_comsym__ = 1;
            a = *((int64_t*) (a ^ TAG_COMSYM));
            print_pair(a);
            printf(")");
            __inside_comsym__ = __inside_comsym;
        }
        break;
    default:
        internal_error();
    }
}

void print_immediate(int64_t a) {
    switch(typeof_mask & a) {
    case TAG_INT:
        printf("%" PRId64, a >> val_shift);
        break;
    case TAG_FLAG :
        if (a & (~typeof_mask)) {
            printf("#t");
        } else {
            printf("#f");
        }
        break;
    case TAG_EMPTY:
        {
            if ((255 & a) == TAG_ELIST)
                printf("()");
            else if ((255 & a) == TAG_ESYMB)
                printf("\'");
            else if ((255 & a) == TAG_ESTRN)
                printf("\"\"");
        }
        break;
    default:
        internal_error(); 
    }
}

void print_pair(int64_t a) {
    if (a == TAG_ELIST) {
        return;
    }
    int64_t car = * ((int64_t*) (a ^ TAG_LIST));
    int64_t cdr = * ((int64_t*) ((a + 8) ^ TAG_LIST));
    print_result(car);
    if (cdr == TAG_ELIST) {

    } else if ((0b111 & cdr) == TAG_LIST) {
        printf(" ");
        print_pair(cdr);
    } else {
        printf(" . ");
        print_result(cdr);
    }
}

int64_t symbol_append(int64_t a, int64_t b) {
    int64_t rv = a;
    a = (a >> 16);
    b = *((int64_t*) (b >> 16)); /* cons bytes */
    uint64_t car = *((int64_t*) a);
    if (car != TAG_ELIST) {
        a = *((int64_t*) a);
        car = *((uint64_t*) ((a ^ TAG_LIST) + 8));
        while (car != TAG_ELIST) {
            a = *((uint64_t*) ((a ^ TAG_LIST) + 8));
            car = *((uint64_t*) ((a ^ TAG_LIST) + 8));
        }
    }
    *((uint64_t*) ((a ^ TAG_LIST) + 8)) = b;
    return rv;
}