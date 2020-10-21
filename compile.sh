#!usr/bin/bash

make poplang
make runtime-bits
./poplang $1 
nasm -felf64 out.s
gcc -no-pie runtime/poplang.o runtime/msc.o runtime/gc.o out.o -o $2
./$2
