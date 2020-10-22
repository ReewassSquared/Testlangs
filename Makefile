UNAME := $(shell uname)
.PHONY: test

ifeq ($(UNAME), Darwin)
  format=macho64
else
  format=elf64
endif

# the following are compiler make directives
compiler/var.o: compiler/var.c compiler/var.h
	gcc -c compiler/var.c -o compiler/var.o

compiler/lex.o: compiler/lex.c compiler/lex.h
	gcc -c compiler/lex.c -o compiler/lex.o

compiler/parse.o: compiler/parse.c compiler/parse.h compiler/lex.h
	gcc -c compiler/parse.c -o compiler/parse.o

compiler/compile.o: compiler/compile.c compiler/compile.h compiler/parse.h compiler/var.h
	gcc -c compiler/compile.c -o compiler/compile.o

compiler/main.o: compiler/main.c compiler/compile.h
	gcc -c compiler/main.c -o compiler/main.o

poplang: compiler/var.o compiler/lex.o compiler/parse.o compiler/compile.o compiler/main.o
	gcc compiler/var.o compiler/lex.o compiler/parse.o compiler/compile.o compiler/main.o -o poplang
	chmod +x poplang

# the following are runtime make directives
runtime/gc.o: runtime/gc.s 
	nasm -f $(format) runtime/gc.s

runtime/msc.o: runtime/msc.c
	gcc -c -fomit-frame-pointer -fno-stack-protector -falign-functions=16 runtime/msc.c -o runtime/msc.o

out.o: out.s
	nasm -f $(format) out.s

runtime/poplang.o: runtime/poplang.c
	gcc -c -fno-stack-protector -falign-functions=16 runtime/poplang.c -o runtime/poplang.o

clean:
	rm -f compiler/*.o runtime/*.o out.s out.o poplang *.exe *.out 

build: poplang

runtime-bits: runtime/gc.o runtime/msc.o runtime/poplang.o
