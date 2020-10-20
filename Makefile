UNAME := $(shell uname)
.PHONY: test

ifeq ($(UNAME), Darwin)
  format=macho64
else
  format=elf64
endif

gc.o: gc.s 
	nasm -f $(format) gc.s

msc.o: msc.c
	gcc -c -fomit-frame-pointer -fno-stack-protector -falign-functions=16 msc.c

undeflang: lex.o parse.o compile.o main.o var.o
	gcc var.o lex.o parse.o compile.o main.o -o undeflang

lex.o: lex.h lex.c
	gcc -c lex.c

parse.o: parse.h lex.h parse.c
	gcc -c parse.c

compile.o: parse.h lex.h var.h compile.h compile.c
	gcc -c compile.c

main.o: parse.h lex.h compile.h main.c
	gcc -c main.c

var.o: var.h var.c
	gcc -c var.c

testcode: undeflang
	./undeflang test.l

compile_test: out.o undeflang.o msc.o gc.o
	gcc -fno-stack-protector -no-pie undeflang.o msc.o gc.o out.o -o test

out.o: testcode
	nasm -f $(format) out.s

undeflang.o: undeflang.c
	gcc -c -fno-stack-protector -falign-functions=16 undeflang.c

clean:
	rm -f *.o undeflang out.s test