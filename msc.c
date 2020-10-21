#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#define MICROHEAP_SIZE (1 << 16)
#define MICROHEAP_FREE(x) (!((x) >> 63))
#define MICROHEAP_MARK(x) ((x) >> 63)

volatile uint64_t __free__ = 0;
extern uint64_t __heapsize__;
extern void* __heap__;
extern uint64_t __gcal__;
extern uint64_t gcbasesz;
extern uint64_t __frame__;
extern void* __root__;
extern uint64_t __gcstack__;
extern uint64_t __globalptr__;
extern uint64_t __globalsize__;
extern uint64_t __gcframe__;
extern void print_result_(int64_t);
extern void print_special(uint64_t a);
int mns_(uint64_t a);
void mns(uint64_t a);
int feauxmns_(uint64_t a);
void feauxmns (uint64_t a);
void debugmns (uint64_t a);
int debugmns_ (uint64_t a);
void mnsweep();
void printdepth_ (int depth);
void prtobj_ (uint64_t a, int depth);

/* uncomment out for debugging */
//#define MNSCFR_TRACE
//#define MNSWEEP_TRACE
//#define GC_DEBUG

static volatile int __tot__ = 0;
static volatile int __totptrs__ = 0;

void mnscfr () {
    //printf("mnscfr: %lu\n", __free__);
#if defined(MNSCFR_TRACE)
    printf("mnscfr DONE: %lu\n", __free__);
#endif
    while ((__free__ <= (__heapsize__ - 256)) && *((uint64_t*) (__heap__ + __free__))) {
        __free__ += 16;
    }
    if (__free__ > (__heapsize__ - 256)) {
#if defined(MNSCFR_TRACE)
        printf("OOM: %lu\n", __free__);
#endif
        return;
    }
}

void foo (uint64_t a) {
    //for (int i = 0; i < (b / 8); i++) {
        //printf("first: $%lu (%%%lx)\n", (*(uint64_t*)a - (uint64_t)__heap__), (*(uint64_t*)a - (uint64_t)__heap__));
       // printf("err $%lu (%%%lx)\n", *(uint64_t*)a, *(uint64_t*)a);
       // a -= 8;
    //}
    printf("one: $%lu (%%%lx)\n", a, a);
    //for (;;);
}

void __gc__ () {
    //printf("\n\n\n======== GARBAGE COLLECTOR ========\n\n\t%d objects marked.\n\t%lu is frame size.\n\t%d stack pointers.\n\n\n", __tot__, __gcframe__, __totptrs__);
    //sleep(1);
    __gcal__++;
#if defined(GC_DEBUG)
    printf("global frame size: %lu\n", __globalsize__);
#endif
    /* first we check globals */
    if (__globalptr__ != 0) {
        uint64_t* globalseg = (uint64_t*) __globalptr__;
        uint64_t gframe = __globalsize__;
        uint64_t g;
        while (gframe) {
            g = ((uint64_t) (*globalseg));

            if (g && (g & 7)) {
                mns(g);
            }
            gframe -= 8;
            globalseg = (globalseg + 1);
        }
    }

    /* then traverse stack */
    uint64_t* stack = (uint64_t*) (__gcstack__ - 8);
    uint64_t frame = __gcframe__;
    uint64_t a;
    while (frame) {
        //a = (uint64_t) stack;
        //printf("\tONSTACK: $%lu (%%%lx)\n", a, a);
        a = ((uint64_t) (*stack));

        /* checks for ignore markers (used to skip over return addresses on stack) */
        if (a == 7) {
            stack = (stack - 2);
            frame -= 16;
            continue;
        }
#if defined(GC_DEBUG)
        printf("%lu ::: $%lu (%%%lx)\n", __gcframe__ - frame, a, a);
#endif
        if (a && (a & 7)) {
            mns(a);
        }
        frame -= 8;
        stack = (stack - 1);
        //if (!(frame % 200)) {
        //    sleep(5);
        //}
    }
#if defined(GC_DEBUG)
    sleep(1);
#endif
    __free__ = 0;
    mnsweep();

#if defined(GC_DEBUG)
    /* first we check globals */
    if (__globalptr__ != 0) {
        uint64_t* globalseg = (uint64_t*) __globalptr__;
        uint64_t gframe = __globalsize__;
        uint64_t g;
        while (gframe) {
            g = ((uint64_t) (*globalseg));

            printf("%lu ::: $%lu (%%%lx)\n", __globalsize__ - gframe, a, a);
            prtobj_(g, 0);
            gframe -= 8;
            globalseg = (globalseg + 1);
        }
    }

    stack = (uint64_t*) (__gcstack__ - 8);
    frame = __gcframe__;
    while (frame) {
        //a = (uint64_t) stack;
        //printf("\tONSTACK: $%lu (%%%lx)\n", a, a);
        a = ((uint64_t) (*stack));

        /* checks for ignore markers (used to skip over return addresses on stack) */
        if (a == 7) {
            stack = (stack - 2);
            frame -= 16;
            continue;
        }
        printf("%lu ::: $%lu (%%%lx)\n", __gcframe__ - frame, a, a);
        prtobj_(a, 0);
        //if (a & 8) {
        //    *stack ^= 8;
        //}
        frame -= 8;
        stack = (stack - 1);
    }
#endif
}

void bar () {
    printf("we did it?\n");
}

/* called by main collector. */
/* rdi (parameter a) contains some stuff */
/* stack is ensured to be preserved after call. */
/* a contains pointer to heap location */
void mns (uint64_t a) {
#if defined(GC_DEBUG)
        debugmns(a);
        return;
#endif
    int tot = mns_(a);
    //printf("err $%lu (%%%lx)\n", a, a);
}

void printdepth_ (int depth) {
    for (int i = 0; i < depth; i++) {
        if (i == (depth - 1)) {
            printf("|-");
            break;
        }
        else {
            printf("  ");
        }
    }
}

void prtobj_ (uint64_t a, int depth) {
    uint64_t b = 0;
    while (a) {
        printdepth_ (depth);
        switch (a & 7) {
        case 1: /* box */
            printf("BOX (%%%lx)\n", a);
            a -= 1;
            if (a & 8) a ^= 8;
            b = *((uint64_t*) a); /*a->box*/
            depth++;
            //a -= 1;
            //tot++;
            break;
        case 2: /* list */
            printf("LIST (%%%lx)\n", a);
            a -= 2;
            if (a & 8) a ^= 8;
            b = *((uint64_t*) a); /*a->left*/
            prtobj_(b, depth + 1);
            a += 8;
            b = *((uint64_t*) a); /*a->right*/
            depth++;
            //a -= 2;
            //tot++;
            break;
        case 3: /* procedure */
            //if (a & 0x8000000000000000) {
            //    
            //    return;
            //    
           // }
           // else {
               if ((a - ((uint64_t) __heap__)) <= __heapsize__) {
                    /* switch over to procenvlist */
                    printf("PROC (%%%lx)\n", a);
                    a -= 3;
                    if (a & 8ULL) a ^= 8ULL;
                    /* switch over to procenvlist */
                    printdepth_(depth + 1);
                    b = *((uint64_t*) a); 
                    printf("PROCD (%%%lx)\n", b);
                    a += 8;
                    b = *((uint64_t*) a); 
                    depth++;
                }
                else {
                    printf("DUMMYPROC (%%%lx)\n", a);
                    a -= 3;
                    if (a & 8ULL) a ^= 8ULL;
                    /* switch over to procenvlist */
                    printdepth_(depth + 1);
                    b = *((uint64_t*) a); 
                    printf("PROCD (%%%lx)\n", b);
                    a += 8;
                    b = *((uint64_t*) a);
                    depth++;
                }
            //}
            //a -= 3;
            //tot++;
            break;
        case 4: /* string */
            printf("STR (%%%lx)\n", a);
            a -= 4;
            if (a & 8) a ^= 8;
            b = *((uint64_t*) a); /*a->strcons*/
            depth++;
            break;
        case 5: /* symbol */
            /* 63:16 = ptr, 15:8 = type, 7:0 = tag */
            /* type is redundant for gc. type will be on lower */
            printf("SYM (%%%lx)\n", a);
            a = (a >> 16);
            //if (a & 8) a ^= 8;
            b = *((uint64_t*) a);
            //tot++;
            depth++;
            break;
        default:
            switch (a & 0b110111) {
            case 0:
                printf("INT %lu%s\n", a >> 6, (a & 8) ? " marked" : "");
                return;
            case 0b010000:
                b = a;
                if (b & 8) b ^= 8;
                printf("FLAG %s%s\n", (b > 16) ? "#t" : "#f", (a & 8) ? " marked" : "");
                return;
            case 0b100000:
                printf("BYTES ");
                for (int i = 1; i < 8; i++) {
                    char __tmp = ((a >> (i*8)) & 0xFF);
                    if (__tmp != 0) {
                        printf("%c", __tmp);
                    }
                }
                printf("%s\n", (a & 8) ? " marked" : "");
                return;
            default:
                printf("EMPTY%s\n", (a & 8) ? " marked" : "");
                return;
            }
        }
        a = b;
        b = 0;
    }
}

int mns_ (uint64_t a) {
    int tot = 0;
    uint64_t b = 0;
    while (a) {
        //printf("entered?\n");
        if (a & 8) return tot;
        switch (a & 7) {
        case 1: /* box */
            a -= 1;
            b = *((uint64_t*) a);
            //if ((b & 7) && (b - ((uint64_t) __heap__)) <= __heapsize__) {
                *((uint64_t*) a) |= 8; /* tag underlying var */
            //    b = b ^ 8; /*a->box*/
            //}
            //a -= 1;
            //tot++;
            break;
        case 2: /* list */
            a -= 2;
            b = *((uint64_t*) a);
            //if ((b & 7) && (b - ((uint64_t) __heap__)) <= __heapsize__) {
                *((uint64_t*) a) |= 8; /* tag underlying var */
              //  b = b ^ 8; /*a->left*/
            //}
            tot += mns_(b);
            a += 8;
             b = *((uint64_t*) a);
            //if ((b & 7) && (b - ((uint64_t) __heap__)) <= __heapsize__) {
                *((uint64_t*) a) |= 8; /* tag underlying var */
              //  b = b ^ 8; /*a->left*/
            //}
            //a -= 2;
            //tot++;
            break;
        case 3: /* procedure */
            //if (a & 0x8000000000000000) {
            //    a ^= 0x8000000000000003;
            //    /* technically a data node */
            //    *((uint64_t*) a) |= 8;
            //    b = *((uint64_t*) a) - 8; /*a->proc*/
            //    return tot;
                
            //}
            //else {
                a -= 3;
                if ((a - ((uint64_t) __heap__)) <= __heapsize__) {
                    /* switch over to procenvlist */
                    b = *((uint64_t*) a);
                    //if ((b & 7) && (b - ((uint64_t) __heap__)) <= __heapsize__) {
                        *((uint64_t*) a) |= 8; /* tag underlying var */
                      //  b = b ^ 8; /*a->left*/
                    //}
                    a += 8ULL;
                    b = *((uint64_t*) a);
                    //if ((b & 7) && (b - ((uint64_t) __heap__)) <= __heapsize__) {
                        *((uint64_t*) a) |= 8; /* tag underlying var */
                       // b = b ^ 8; /*a->left*/
                    //}
                }
                else {
                    tot += 1;
                    return tot;
                }
            //}
            //a -= 3;
            //tot++;
            break;
        case 4: /* string */
            a -= 4;
            b = *((uint64_t*) a);
            //if ((b & 7) && (b - ((uint64_t) __heap__)) <= __heapsize__) {
                *((uint64_t*) a) |= 8; /* tag underlying var */
               // b = b ^ 8; /*a->left*/
            //}
            //tot++;
            break;
        case 5: /* symbol */
            /* 63:16 = ptr, 15:8 = type, 7:0 = tag */
            /* type is redundant for gc. type will be on lower */
            a = (a >> 16);
            b = *((uint64_t*) a);
            //if ((b & 7) && (b - ((uint64_t) __heap__)) <= __heapsize__) {
                *((uint64_t*) a) |= 8; /* tag underlying var */
            //b 8; /*a->left*/
            //}
            //tot++;
            break;
        default:
            a = 0; /* leaf reached, get the hell out of dodge */
        }
        if ((a != 0) && ((b & 7) == 0)) {
            /* b is a data node, so a points to data. */
            //if ((a - ((uint64_t) __heap__)) <= __heapsize__) {
                *((uint64_t*) a) |= 8; 
            //}
            //tot++;
            return tot;
        }
        else {
            a = b;
            b = 0;
        }
    }
    return tot;
}

void debugmns (uint64_t a) {
    printf("OBJECT:\n");
    prtobj_(a, 0);
    int tot = debugmns_(a);
    printf("MARKED OBJECT:\n");
    prtobj_(a, 0);
    printf("\tOBJECT:: %d subtrees.\n", tot);
    __tot__ += tot;
    if (a & 7) __totptrs__++;
    //printf("err $%lu (%%%lx)\n", a, a);
}

int debugmns_ (uint64_t a) {
    int tot = 0;
    uint64_t b = 0;
    while (a) {
        //printf("entered?\n");
        if (a & 8) return tot;
        switch (a & 7) {
        case 1: /* box */
            a -= 1;
            b = *((uint64_t*) a);
            //if ((b & 7) && (b - ((uint64_t) __heap__)) <= __heapsize__) {
                *((uint64_t*) a) |= 8; /* tag underlying var */
             //   b = b ^ 8; /*a->box*/
            //}
            //a -= 1;
            tot++;
            break;
        case 2: /* list */
            a -= 2;
            b = *((uint64_t*) a);
            //if ((b & 7) && (b - ((uint64_t) __heap__)) <= __heapsize__) {
                *((uint64_t*) a) |= 8; /* tag underlying var */
              //  b = b ^ 8; /*a->left*/
            //}
            tot += debugmns_(b);
            a += 8;
             b = *((uint64_t*) a);
            //if ((b & 7) && (b - ((uint64_t) __heap__)) <= __heapsize__) {
                *((uint64_t*) a) |= 8; /* tag underlying var */
              //  b = b ^ 8; /*a->left*/
            //}
            //a -= 2;
            tot++;
            break;
        case 3: /* procedure */
            //if (a & 0x8000000000000000) {
            //    a ^= 0x8000000000000003;
            //    /* technically a data node */
            //    *((uint64_t*) a) |= 8;
            //    b = *((uint64_t*) a) - 8; /*a->proc*/
            //    return tot;
                
            //}
            //else {
                a -= 3;
                if ((a - ((uint64_t) __heap__)) <= __heapsize__) {
                    /* switch over to procenvlist */
                    b = *((uint64_t*) a);
                   // if ((b & 7) && (b - ((uint64_t) __heap__)) <= __heapsize__) {
                        *((uint64_t*) a) |= 8; /* tag underlying var */
                      //  b = b ^ 8; /*a->left*/
                    //}
                    a += 8ULL;
                    b = *((uint64_t*) a);
                    //if ((b & 7) && (b - ((uint64_t) __heap__)) <= __heapsize__) {
                        *((uint64_t*) a) |= 8; /* tag underlying var */
                      //  b = b ^ 8; /*a->left*/
                   // }
                }
                else {
                    tot += 1;
                    return tot;
                }
            //}
            //a -= 3;
            tot++;
            break;
        case 4: /* string */
            a -= 4;
            b = *((uint64_t*) a);
            //if ((b & 7) && (b - ((uint64_t) __heap__)) <= __heapsize__) {
                *((uint64_t*) a) |= 8; /* tag underlying var */
            //    b = b ^ 8; /*a->left*/
            //}
            tot++;
            break;
        case 5: /* symbol */
            /* 63:16 = ptr, 15:8 = type, 7:0 = tag */
            /* type is redundant for gc. type will be on lower */
            a = (a >> 16);
            b = *((uint64_t*) a);
            //if ((b & 7) && (b - ((uint64_t) __heap__)) <= __heapsize__) {
                *((uint64_t*) a) |= 8; /* tag underlying var */
             //   b = b ^ 8; /*a->left*/
            //}
            tot++;
            break;
        default:
            a = 0; /* leaf reached, get the hell out of dodge */
        }
        if ((a != 0) && ((b & 7) == 0)) {
            /* b is a data node, so a points to data. */
            //if ((a - ((uint64_t) __heap__)) <= __heapsize__) {
                *((uint64_t*) a) |= 8; 
            //}
            tot++;
            return tot;
        }
        else {
            a = b;
            b = 0;
        }
    }
    return tot;
}

void mnsweep () {
#if defined(GC_DEBUG)
    printf("\n\n\n======== GARBAGE COLLECTOR ========\n\n\t%d objects marked.\n\t%lu is frame size.\n\t%d stack pointers.\n\n\n", __tot__, __gcframe__, __totptrs__);
    sleep(2);
#endif
    __tot__ = 0;
    __totptrs__ = 0;
    uint64_t a = (uint64_t) __heap__;
    int sz = __heapsize__ / 16;
    uint64_t b;
#if defined(GC_DEBUG)
    printf("GC SIZE: %lu, OBJ COUNT: %d\n", __heapsize__, sz);
#endif
    for (int i = 0; i < sz; i++) {
        //printf("mnsweep: %07d\n", i);
        /* remember to check for special case procd */
        b = *((uint64_t*) a);
#if defined(MNSWEEP_TRACE)
        printf("OBJ(%04d): %lx, %lx\n", i*16, b, *((uint64_t*) (a + 8))); 
#endif
        if (b & 8ULL) {
            *((uint64_t*) a) ^= 8ULL; /* unset flag */

            a += 8ULL;
            b = *((uint64_t*) a);
            if (b & 8ULL) {
                *((uint64_t*) a) ^= 8ULL;
            }
            a += 8ULL;
        }
        else {
            if (__free__ == 0) {
               __free__ = i*16;
            }
            *((uint64_t*) a) = 0;
            *((uint64_t*) (a + 8ULL)) = 0;
            a += 16ULL;
        }
    }
    if (__free__ & 8) __free__ += 8;
#if defined(GC_DEBUG)
    printf("\n\n\n======== GARBAGE COLLECTOR ========\n\n\t%li new offset.\n\n\n", __free__);
    sleep(1);
#endif
}
