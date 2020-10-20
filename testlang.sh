#!usr/bin/bash
./undeflang $1
gcc -c undeflang.c
nasm -felf64 out.s
gcc -no-pie undeflang.o out.o -o $2
