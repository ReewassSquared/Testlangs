#!usr/bin/bash

make poplang
make runtime-bits
./poplang $1 
gcc -no-pie undeflang.o msc.o gc.o out.o -o $2
./$2
