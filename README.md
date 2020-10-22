# Poplang

Poplang is a testbed for the Poplarian Programmer project, a general-purpose functional programming language and environment geared towards scalability and cultism.

The language is designed to support multiple style conventions while providing the same backend, but currently Poplang exists as a primitive lisp. Ultimately, Poplang will provide imperative tools, strict typing tools, and other development frameworks, and multiple syntaces (i.e. ML-style, Haskell-style, C-style) with a tool called the "syntax baker." However, for now, Poplang is heavily lisp-based.

Table of Contents
=================
0. [Installation](#installation)
1. [Repository Organization](#repository-organization)
    * [Compiler](#compiler)
    * [Runtime](#runtime)
    * [Scripts](#scripts)
2. [Language Description](#language-description)
    * [Types](#types)
    * [Operations](#operations)
    * [Expressions](#expressions)
    * [Programs](#programs)

# Installation

To install, clone this repository. Currently, the compiler only supports linux environments, as this is the environment it was coded in. You can compile poplang scripts by calling:

```
. compile.sh <path of script> <name of executable to output>
```

Currently only one script can be compiled into an executable.

# Repository Organization

Poplang's repository is currently under a refactor, but the following conventions should hold regardless. Currently, the code is being hosted on [GitHub](https://github.com/ReewassSquared/Testlangs) but may be moved at a later date. The aim of the project may also change some of the repository structure, and as our team grows the scope of the project might as well.

## Compiler

This directory contains the lisp-style syntax lexer tools, parser, definition table, and code generation tools. As of now, the compiler for Poplang uses three lexers: one for parsing s-expressions, one for parsing quasiquoted expressions, and one for parsing fully quoted expressions. The code for the lexer can be found in [lex.c](https://github.com/ReewassSquared/Testlangs/blob/master/compiler/lex.c) and [lex.h](https://github.com/ReewassSquared/Testlangs/blob/master/compiler/lex.h), and the code for the definition tables can be found in [var.c](https://github.com/ReewassSquared/Testlangs/blob/master/compiler/var.c) and [var.h](https://github.com/ReewassSquared/Testlangs/blob/master/compiler/var.h).

The parser currently operates under a single initial stage, where all lexical blocks are assembled into nodes on the AST. The AST itself is not too fancy and somewhat bloated, and there are no optimizations performed on the AST currently. There are significant AST transformations, however, such as putting `list` into ANF binary operations of `cons`, as well as converting certain variable expressions (`and`, `or`) into nested `if` expressions. Additionally, pattern `match` expressions are converted to `cond` expressions. The code for the parser and data model for the AST can be found in [parse.c](https://github.com/ReewassSquared/Testlangs/blob/master/compiler/parse.c) and [parse.h](https://github.com/ReewassSquared/Testlangs/blob/master/compiler/parse.h).

The compiler itself does not perform anything other than code generation. It outputs an assembly file (Intel syntax, designed for assembly with NASM) to be further compiled into object code and linked with the runtime to form an executable. The code generation provides significant support for the automatic memory manager for the runtime. For example, non-tail calls write a magic number to the stack to inform the stack traversal algorithm for scavenging roots to skip the next machine word on the stack. Additionally, a global which tracks the frame size is updated significantly. 

The compiler outputs a significant amount of assembly instructions, and overall is bloated in implementation and in output. Our hope is to apply further optimizations to the AST and limit the amount of work the code generator needs to do, while making the runtime automatic memory manager better at managing runtime information such as active roots and frame sizes.

The code for the compiler/code generator can be found in [compile.c](https://github.com/ReewassSquared/Testlangs/blob/master/compiler/compile.c) and [compile.h](https://github.com/ReewassSquared/Testlangs/blob/master/compiler/compile.h), and the main file for the compiler is [main.c](https://github.com/ReewassSquared/Testlangs/blob/master/compiler/main.c).

While it was not mentioned previously, there are many debugging tools for lexer tracing, parser tracing, token consumption tracing, printing the AST, printing a compilation trace, printing a definition table trace, and more that can be set by changing the appropriate variables.

## Runtime

The runtime for Poplang is primitive, currently limited to 64-bit systems. The entry point is the `main` function found in `poplang.c`, which performs an initial allocation of the heap using C's `calloc`. Additionally, the frame size variable `__gcframe__` is initialized at zero before calling the entry point in the code generated by the compiler. After returning, the heap is freed and the program exits with status 0. 

Poplang provides basic error behaviour, where failed type assertions and other runtime errors print an error value followed by an error code. There is no current standard for runtime error behaviour specified, although partial error standards exist in the language. For failed type assertions, the error value contains the operand which failed the type assertion, while the error code is either `64 * variable_tag` for pointer types, or `2 * variable_tag` for non-pointer types. Additionally, when the heap is exhausted of available memory, the error code is `0xA0` and the error value is the current free pointer offset.

Poplang runtime prints the return value of all global scope returning s-expression evaluations, unless followed by a gereed. Additionally, the runtime itself provides the routines for comparing string and primitive symbol equality, as well as file operations and symbol appending. All the code so far can be found in [poplang.c](https://github.com/ReewassSquared/Testlangs/blob/master/runtime/poplang.c).

The automatic memory manager consists of a memory standard as well as a root collection algorithm, a root traversal algorithm, a marking algorithm, and a garbage collector. The roots exist as pointer variables on the stack and as pointer variables in the 16-byte aligned data segment. Whenever a collection is incurred, the global roots are traversed first and marked, then the stack is traversed and marked. Finally, all marks are cleared with garbage cleared to zero during the sweep phase. Essentially, the automatic memory manager is a mark-and-sweep garbage collector, where all allocations are sixteen bytes. 

There is a significant amount of debugging options for the garbage collector, which can be changed via the commented-out preprocessor definitions.

## Scripts

There are a number of testing scripts for the language found in this directory. `test*.l` are files that were used extensively during initial tests of the language, with the other files used for testing the later features. You can PR any of your Poplang scripts into this directory if you so please as well.


# Language Description

Poplang is a functional programming language with dynamic typing, with lisp syntax and ahead-of-time compilation. In memory, the programs consist of a code and data segment, as well as a stack and a heap (which are both automatically managed). 

The language allows for non-definition and non-builtin symbols (i.e. symbols used for variable bindings in `let` and `match`) to be shadowed. Builtins consist of the following symbols, which should do not shadow: `cond abs if else zero? add1 sub1 print let box unbox cons car cdr define lambda string? integer? flag? boolean? list? emptylist? box? proc? bytes? symbol? string=? symbol=? and or length list mod bitand bitor bixor bitshl bitshr not match cons? set! file-open file-write file-close symbol-append _`, and the following which are depracated but reserved: `fun call`. Reserved symbols for the language are `[`, `(`, `)`, `]`, `'`, `~`, `=`, `>=`, `<=`, `<`, `>`, `?`, `#`, `+`, `-`, `*`, `/`, `;`, `"`, and backtick. Identifiers must begin with an alphanumeric or underscore character, and may be followed by non-whitespace characters which do not consist of the following: `(`, `)`, `[`, `]`, `;`, `"`. 

## Types

Poplang is dynamically typed, meaning variables consist of both data and some form of metadata. Usually, variables exist as either flat data or pointers, followed by a variable tag. Bit three is always reserved as the mark bit for the memory manager, with the lower three bits being the 'lower tag' and any bits left of the mark bit being 'upper tag' (although the upper tag might not be in use).

In Poplang, there are only four primitive non-pointer data types: Integers, Bytes, Booleans/Flags, and the empty marker. Booleans are the lowest order Flag types, with the upper flag value of 0 being `#f` and the upper value of 1 being `#t`, and the rest are reserved. Primitives have a three-bit tag of zero, with an upper tag consisting of two bits left of the mark bit. This tag is zero for integers, one for flags, two for bytes, and three for empty.

Integers are arithmetic signed data which must fit into 58 bits of memory. Flags are unsigned data which may fulfill multiple roles in the future, but currently serve as booleans. Bytes currently are only used by strings and primitive symbols to store character data, with the upper seven bytes used possibly for characters and the lower byte storing the tag. Finally, the empty type is used solely as the empty list right now.

Pointer types come in the form of Boxes, Lists, Procedures, Strings, and Symbols. Save for the Symbol, pointer types only use a lower tag, which is 1 for Boxes, 2 for Lists, 3 for Procedures, 4 for Strings, and 5 for Symbols. Symbols store their pointer in the upper 48 bits, with the penultimate byte storing metadata regarding how to process the symbol on further quote and unquote operations. 

Boxes are special types which function as a pointer to another type, allowing behaviour similar to non-functional programs. Currently, the full scope of their capability is not implemented yet.

Lists consist of a pointer to two adjacent machine words in memory, with the first one being 16-byte aligned. Lists are a `cons` of these two machine words, where `car` retrieves the first word and `cdr` retrieves the second. The empty non-pointer type marks an empty list or a list terminal.

Procedures are closures, which point to a 16-byte aligned chunk of memory 16 bytes in size. The first eight bytes is a tagged pointer to the instruction to jump to when executing the procedure. The proceeding eight bytes is a List where the `car` is the data to use as the first implicit argument (i.e. first free variable), and `cdr` is either an empty or a List, etc., i.e. a linked list of free variables.

Strings are similar to Procedures, in that they point to a 15-byte aligned chunk of memory 16 bytes in size. The first eight bytes is a List, linked list of bytes that store the character date in major order, where nul characters are fully ignored. The last eight bytes are reserved.

Symbols are similar to Strings. The penultimate byte stores the type tag of the quoted value, meaning the pointer points to a variable of that type. However, when the penultimate byte is 255 (0xFF), this pointer instead points to a linked list of bytes the same as a String.

Type assertions are performed at runtime for operations, and type checks can be performed using the appropriate primitive expression.

## Operations


