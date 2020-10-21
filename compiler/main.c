#include <stdio.h>
#include "compile.h"

int main (int argc, char** argv) {
    if (argc != 2) {
        printf("err: invalid arguments to compiler.\n");
    } else {
        compile(argv[1]);
        return 0;
    }
}
