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
        * [abs](#operations-abs)
        * [add1, sub1](#operations-incdec)
        * [bitand, bitor, bitxor, bitshl, bitshr](#operations-bit)
        * [>=, <=, >, <](#operations-compare)
        * [=, ~](#operations-eq)
        * [type?](#operations-typecheck)
        * [zero?](#operations-zero)
        * [mod](#operations-modulus)
        * [not](#operations-logicnot)
        * [string=?, symbol=?](#operations-charcmp)
        * [symbol-append](#operations-append)
        * [length](#operations-length)
        * [print](#operations-print)
        * [unbox, car, cdr](#operations-composite-get)
        * [+, -, *, /, and, or](#operations-variadic)
        * [set!](#operations-side-effect)
    * [Constructors](#constructors)
        * [box](#constructors-box)
        * [cons, list](#constructors-cons)
        * [lambda](#constructors-lambda)
        * [Symbols](#constructors-syms)
    * [File I/O](#file-ops)
        * [file-open](#file-open)
        * [file-write](#file-write)
        * [file-close](#file-close)
    * [Binding and Conditionals](#binding-conditionals)
    * [Miscellaneous](#misc)
3. [Roadmap](#roadmap)

# Installation

To install, clone this repository. Currently, the compiler only supports linux environments, as this is the environment it was coded in. You can compile poplang scripts by calling:

```bash
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

Operations are primitive expressions which could be semantically replaced by a function call. They are a special kind of expression that provide a builtin where possible as well. As stated previously, keywords cannot be shadowed. However, this will not be the case forever. Eventually, keywords can be shadowed as well. So you can picture Operations not as combinations of keywords and arguments but as function calls that compile to a inline builtin instead.

### abs

`abs` takes in a single argument and returns the absolute value of that argument. This operation incurs a type assertion, where the argument must be of type integer, and will cause a runtime error when the type assertion fails.

For example, the following expressions:
```racket
(abs 1)
(abs -1)
(abs #t)
(abs (add1 -1))
```
Will return 1 and 1, with the next one throwing a runtime error with error code 0x00, and the fourth one returning 0.

### add1, sub1

`add1` and `sub1` both take in a single argument and return the increment and decrement, respectively. This operation incurs a type assertion, and the argument must be an integer.

Examples:
```racket
(add1 0)
(add1 7)
(sub1 (add1 -1))
```
Will return 1, 8, and -1, respectively. 

### bitand, bitor, bitxor, bitshl, bitshr

The following operations take in two arguments and perform type assertions on both, where the arguments must both be integers. They perform the arithmetic bitwise `and`, `or`, `xor`, `shl`, and `shr`, respectively.

Examples:
```racket
(bitand 5 3)
(bitor 1 2)
(bitxor 7 2)
(bitshl 20 1)
(bitshr 20 1)
```
Will return 1, 3, 5, 40, and 10, respectively. Undefined behaviour occurs when the `bitshl` and `bitshr` exceed the 58-bit range.

### >=, <=, >, <

The following operations take in two arguments and perform type assertions on both, where the arguments must both be integers. They perform their traditional comparisons, where `>=` returns `#t` when argument 1 is greater than or equal to argument 2 and `#f` otherwise; `<=` returns `#t` when argument 1 is less than or equal to argument 2, and `#f` otherwise; `>` returns `#t` when argument 1 is greater than argument 2, and `#f` otherwise; and `<` returns `#t` when argument 1 is less than argument 2, and `#f` otherwise.

Examples:
```racket
(> 1 2)
(< 2 2)
(<= 2 2)
(>= 3 2)
```
Will return `#f`, `#f`, `#t`, and `#t`, respectively. If the fourth expression instead had its arguments swapped with one another, it would return `#f`. The same goes for the first expression. 

### =, ~

The following operations take in two arguments which do not necessarily need to be of the same type. They perform physical equality tests on their arguments. `=` returns `#t` if its first argument is physically equal to the second argument (i.e. non-pointer equality and for pointers, if they both point to the same address and have the same tag information), and `#f` otherwise. `~` returns `#f` when both arguments are physically equal to one another, and `#t` otherwise (not-equals, essentially).

Examples:
```racket
(= 1 2)
(= #t #t)
(= "a" "a")
(~ "a" "b")
(let ((x "foo")) (= x x))
(let ((x "bar")) (~ x x))
```
The first expression returns `#f` since 1 and 2 as represented in Poplang are not equivalent. However, the second expression returns `#t` since `#t` is represented the same way for all representations in memory of `#t`. The third expression returns `#f` since `"a"` is not represented the same in memory, and two literal strings will always be unequal pointers. The fourth expression returns `#t` by the same logic used in the third expression. The fifth expression returns `#t` since a variable must always be equal to itself, and the final expression returns `#f` by the same logic for the previous expression.

### type?

Type check operations exist for all types in Poplang, and all take a single argument. They all return `#t` if their argument is of that type, and `#f` otherwise. `integer?`, `box?`, `flag?`, `emptylist?`, `proc?`, `bytes?`, `string?`, and `symbol?` simply check if the lower tag (and for non-pointers, upper tag too) matches the specified type check. For `cons?`, this checks if the lower tag matches the tag used for the List pointer type. For `list?`, this is equivalent to `(or (cons?) (emptylist?))`. For `boolean?`, this is functionally the same as `(or (= #t) (= #f))`. 

Examples:
```racket
(define (h a b) (if #t a b))
(integer? 2)
(integer? #t)
(list? (cons "a" 2))
(list? ())
(proc? (lambda (x) x))
(proc? h)
(cons? ())
(emptylist? (list 1 2 3))
(symbol? '(foo))
(symbol? 'foo)
(flag? #t)
```
Ignoring the definition expression, the first expression returns `#t` while the next expression returns `#f`. The two following expessions both return `#t`, as well as the next two checking for procedure types, which proves that function definitions are also procedures and therefore produce a closure (albeit the closure's free variable argument list is just the empty list). The next one returns `#f` since the empty list is not a List type, but an empty type. By that same logic, the next expression returns `#f` since a List type is not an empty type. The final three return `#t`.

### zero?

This operation takes in a single integer argument and performs type assertion first. It then returns `#t` if the argument is equal to 0, and `#f` otherwise. 

### mod

This operation takes in two integer arguments and performs type assertion on both. It then returns the arithmetic modulus of the first argument by the second argument. Behaviour for modulus by zero is undefined.

### not

This operation takes in a single boolean argument and performs type assertion first. It returns `#t` when the argument is `#f`, and `#f` if the argument is `#t`. It's basically the logical not operation for the boolean values `#t` and `#f`.

### string=?, symbol=?

These operations perform structural equality on their specified types. `string=?` returns `#t` if the two arguments represent the same string structurally (i.e. all non-nil characters in their underlying bytes linked lists must be in the same order and equal to one another), and `#f` otherwise. `symbol=?` functions in the same way, but only performs structural equality when both arguments are primitive symbols, and returns `#f` if one or more of its arguments are not primitive symbols. These take in two arguments, and perform type assertion on both. 

Examples:
```racket
(symbol=? 'a 'b)
(symbol=? '(a) 'b)
(symbol=? '(a) '(a))
(symbol=? 'abc (symbol-append 'ab 'c))
(string=? "a" "a")
```
The first expression returns `#f` because `'a` and `'b` are not structurally equal. The second expression and third expressions return `#f` since they both contain arguments which are not primitive symbols. The fourth expression returns `#t` since both arguments are structurally the same. The fifth expression also returns `#t`.

### symbol-append

This operation takes in two symbols and returns the first symbol followed by the second symbol. This performs type assertion, and expects both arguments to be primitive symbols. This also produces a side effect, where the first argument is modified.

### length

This operation takes in a List and returns the right-recursive length of the list. It performs type assertion on the argument initially, and can also take in the empty list. 

Examples:
```racket
(length ())
(length (cons 1 2))
(length (cons 1 (cons 2 ())))
(length (list 1 2 3 4))
(length (cons 1 (cons (cons 2 3) (cons 4 5))))
```
The first expression returns 0. The second expression returns 1 since the righthand element of the List isn't another List. The third expression returns 2 since the righthand element of the List is a second List, and the righthand element of that proceeding List is not another List. The fifth expression also returns 2 for the same reason. The fourth expression returns 4 since `(list 1 2 3 4)` will return `(cons 1 (cons 2 (cons 3 (cons 4 ()))))`, which is right-recursive List of length four. 

### print

This operation takes in any arbitrary argument and prints it to `stdout` (usually the terminal). It returns `#t`, always.

### unbox, car, cdr

These operations recover information stored in Box types or List types. `unbox` takes in a single argument which undergoes type assertion to ensure it is Box type, and returns the boxed data stored inside the argument. `car` and `cdr` take in a single argument asserted to be of type List (not the empty list), where `car` returns the lefthand element of the List, and `cdr` returns the righthand argument of the List.

### +, -, *, /,  and, or

These operations take in one or more arguments. `+`, `-`, `*`, and `/` assert all their arguments are of type integer, while `and` and `or` assert all their arguments are of type boolean.

`+`, `*`, and `/` return the absolute value of their argument if there is only one argument given, while `-` computes the negation of its argument if there is only one argument given. For multiple arguments, the in-order series is applied, where `+` computes the left-to-right sum of its arguments, `*` computes the left-to-right product, `/` computes the left-to-right division of its arguments, and `-` computes the left-to-right subtraction of its arguments. If any arguments besides the first argument are equal to zero for `/`, undefined behaviour occurs.

`and` returns the logical and for boolean types `#t` = true and `#f` = false, while `or` returns the logical or for these boolean types. The calculation is applied in order of left-to-right, so any side effects will be produced in that order.

Examples:
```racket
(+ -1 2 3)
(+ -1)
(- 1)
(- 1 2 3)
(* -1 -2)
(/ 1 2 0)
(and #t #f)
(or #t #f)
```
The first expression returns 5, while the second returns 1 and the third returns -1. The fourth expression returns -4, and the fifth expression returns 2. The sixth expression has undefined behaviour. The seventh expression returns `#f` and the eighth expression returns `#t`.

### set!

This is the final operation implemented thus far, which takes in one argument which must be a lexical symbol (i.e. global variable, argument, in-scope local binding, or in-scope lambda argument) followed by any arbitrary argument (so it takes in two arguments total). This expression returns the second argument, but also produces a side-effect where the lexical symbol specified now stores the value returned by or stored in the second argument. If the first argument is the name of a function definition, undefined behaviour occurs.

Examples:
```racket
(let ((x 2)) (let ((y (set! x 3))) x))
(define myglobal 2)
(define (identity x) x)
(set! myglobal #t)
myglobal
(set! identity #f)
```
The first expression mutates the value stored in `x` when evaluating the binding expression for `y` to be 3, so this returns 3. Ignoring the definitions, the second expression mutates the value stored in `myglobal` to be `#t`, and returns `#t`. The third expression returns `#t` since the previous expression mutated `myglobal` to store this value. The final expression produces undefined behaviour.

## Constructors

In Poplang, composite types such as Box, List, Procedure, and non-primitive Symbol have constructors. These constructors are basic, but allow for complex behaviour to occur when used together with expressions. Thus, Poplang allows for very high-level programming with lots of expressiveness, and at the core of this are these underlying constructors.

### box

This constructor takes in an argument of any type, and allocates space for it onto the heap. It then returns a pointer to this value, which is tagged to be type Box. 

While this type is pretty much useless right now, it will in the future only move it to the heap if the underlying value escapes the scope of its live expression after boxing, and the box will instead be a pointer to the actual value itself, wherever it might be. Currently, this functionality is not complete yet, and tools for escape analysis and code generation for updating the boxed data while in scope must be implemented. 

Additionally, in the future, `box-set!` will be an operation similar to `set!` but instead operate on lexical symbols or expressions as the first argument and, after asserting the first argument produces a box type, mutate the boxed data to be instead the second argument's return value, then returns this updated value.

### cons, list

These constructors create lists. `cons` expects exactly two arguments of any type, and allocates space on the heap to store these two values. Then, it returns a pointer, tagged as a List, to this space. 

List can take zero or more arguments of arbitrary type and produces a right-recursive linked list of the arguments. 

Examples:
```racket
(cons 1 2)
(cons "a" (cons "b" ()))
(list)
(list #t)
(list #t #f (cons 'sym ()))
```
The first expression returns a List with lefthand element being 1 and righthand element being 2. The second expression returns a List wherethe lefthand element is `"a"` and the righthand element is a List, with its lefthand element being "b" and the righthand being the empty list. The third expression returns the empty list, while the fourth expression returns the same thing as `(cons #t ())`. The final expression returns the same thing as `(cons #t (cons #f (cons 'sym ())))`.

### lambda

Lambdas are important in many languages because they represent first-class functions coupled with their own environments. They are closures, which allow both execution of a routine with its own environment of implicit arguments and can take explicit arguments too. 

In Poplang, lambdas are closures to the fullest degree. Thus, they capture all free variables necessary in their underlying structure for use later, and can be called the same as function definitions. Additionally, function definitions themselves are closures, except their environment is essentially empty. Therefore, lambdas and functions are interchangeable, and are functionally equivalent.

One of the nice features of lambdas is that when their implicit arguments are mutated, they mutate the closure environment as well to reflect the argument's mutation. Unfortunately, Poplang does not do this yet, but it is planned to be implemented soon.

To disambiguate, lambdas and function definitions are both procedure types, with all procedure types being closures.

Examples:
```racket
(lambda (x) x)
((lambda (x y) (+ x y)) 1 2)
(define (myfun x) (+ x x))
((lambda (f) (f 2)) myfun)
myfun
(lambda (x) (lambda (y) x))
```
The first expression returns a procedure that takes in a single explicit argument and returns that argument. The second expression calls the first argument, which is a procedure that takes two explicit arguments and returns their arithmetic sum, with the first and second arguments being 1 and 2, respectively. So the second expression returns 3. Skipping the third expression, the fourth expression calls the first argument, which is a procedure that takes in a single explicit argument, and calls that argument with a single argument of 2. The single argument to the fourth expression call is `myfun`, which itself is a procedure that takes in a single argument and returns the arithmetic sum with itself. Thus, the fourth expression returns 4. The fifth expression returns a procedure for the function definition `myfun`. The final expression returns a procedure with one explicit argument, which will return a procedure with one explicit argument `y` and one implicit argument `x` captured from the first procedure, which will returns the implicit argument.

### Symbols

Symbols come in two varieties: primitive and composite. Primitive symbols exist as string-like entities that can behave as qualifiers for disambiguating structured data. Composite symbols are entirely separate from their primitive counterparts, and exist as a "quoted" data type. 

For instance, `'abc` is a primitive symbol, while `'7` is not. When symbols are compiled, they are aggressively checked if they could possibly be unquoted into a valid type, such as a string, integer, boolean, or list. If they can be unquoted into any of these types, then they are compiled as composite symbols, which instead symbolically "wrap" the underlying data. 

So `'"abc"` does not produce a primitive Symbol which stores the character data for the string quotation, followed by the characters `abc` followed by a string quotaton; instead, it stores a composite Symbol which itself wraps a string with the character data `abc`. The same goes for `'192`, which instead stores a composite Symbol wrapping the integer 192. 

When a quotation contains parentheses, it represents a special composite symbol that wraps a linked list of all the quoted data inside. So `'(7 a b c)` is actually the result of quoting `(list '7 'a 'b 'c)`. 

While all types can be quoted (including Boxes, Procedures, and Symbols!), not all these types have a way of declaring them literally in source code (like Boxes and Procedures). 

There are two ways of constructing Symbols: quote (which uses the single quotation `'`) and quasiquote (which uses the backtick). Literal symbols are prepared using either operation, but quasiquote allows for more expressiveness. Whenever a quasiquote construction is performed, while in scope of the quasiquote operation, a single comma `,` specifies that the next expression is to be evaluated before quoting, and is called the unquote operator. This allows quasiquote construction to be very high-level. Unquotes cannot be used with quote construction.

The comma `,` can also be used outside the scope of quasiquote construction and function as an unquote operation, whith unwraps the composite Symbol and returns the wrapped data. Additionally, regardless if it is within or outside of a quasiquote construction, unquote can operate on all symbol types. This includes primitive symbols, and is essentially just an identity operation.

Examples:
```racket
'abc
'"abc"
'"ab"c
'7
'2809387460217346102837460912340
'(a b c)
`(node ,(add1 7) ,(cons "a" "b"))
,'7
,'abc
,'(a b c)
```
The first expression constructs a primitive symbol `'abc`. The second constructs a composite symbol which wraps the string `"abc"`. The third creates a primitive symbol with the character data `"ab"c` stored inside, since the data contained within the quote constructor does not represent an unquotable type. The fourth expression creates a composite symbol wrapping the integer 7. The fifth expression creates a primitive symbol with all that character data, since the underlying data cannot be unquoted into a valid integer within range of the integer type. The sixth expression creates a composite symbol that is equivalent to `(list 'a 'b 'c)` when unquoted. T

The next expression is a quasiquote constructor, which creates a composite symbol wrapping a list with the first element being `'node`, and the second element being the result of `(add1 7)` (8), and the third being the result of `(cons "a" "b")` (`("a" . "b")`). The following expression unquotes the quoted 7, thereby returning 7. The next expression returns `'abc`, since unquoting a primitive symbol returns itself. The final expression returns `(cons 'a (cons 'b (cons 'c ())))`.

## File I/O

File I/O in Poplang is more of a novelty than anything else. While a file is open, no other files can be opened (i.e. only one file can be open at any given time).

There are currently only the following operations that can be performed regarding File I/O.

### file-open

This operation takes in a single string argument and sets the open file to be the file resulting from the path the character data of the string represents. This operation returns `#t` on success, and `#f` otherwise. Additionally, this operation opens the file in `w+` mode, meaning any previous data in the file is overwritten.

### file-write

This operation takes in a single argument and writes some form of the data represented in the argument to the file. This returns `#t` on success, and `#f` otherwise.

### file-close 

This closes the current file, and takes in no arguments. This returns `#t` on success, and `#f` otherwise.

## Binding and Conditionals

Conditionals in Poplang are either `if` expressions or the composite `cond` expression. However, Poplang also supports a degree of pattern matching, where `match` is also a form of conditional expression.

`if` takes in three arguments, where the first argument is asserted to be a boolean. If the first argument is `#t`, then the second argument is returned. Otherwise, the third argument is returned.

`cond` is called with a parenthetically encapsulated list of s-expression conditional clauses. For each clause, beginning with the first one, the first argument is asserted to be a boolean, and if `#t`, returns the second argument. Otherwise, the proceeding clause is evaluated. `cond` expressions must terminate their s-expression list with an `else` expression, which takes in a single argument and returns this.

`match` performs traditional pattern matching on its specified argument, and instead takes in a one argument followed by a bare s-expression list of pattern clauses. For each clause, the first argument is evaluated via the pattern evaluator, which returns `#t` if the main `match` argument matches the specified pattern. If `#t`, then the second argument is returned. Otherwise, the proceeding clause is evaluated. If the pattern is `_`, this always returns `#t`. If the pattern is an s-expression with the first argument being `?`, this calls second argument, which is presumed to be either a unary builtin operation or a unary procedure, with the single argument passed in being the main `match` argument, and returns `#t` if the call returns `#t`. If this is followed by an identifier, the identifier is bound with the value stored inside the `match` argument.

If you've noticed, `match` can bind variables. There is one more expression which can bind variables: `let`, which is the canonical binding expression. This takes in a parenthetically encapsulated s-expression list of variable binding clauses. For each clause, the first argument (which must be an identifier) is bound to the second argument. Finally, the `let` expression takes a second argument, which itself is an expression, where all the bound variables are in scope for the expression.

## Miscellaneous

All expressions which return a value will print that value to `stdout` in its printed if the expression is not supressed and resides at the global scope. 

Whitespace is meaningless in Poplang programs, besides separating identifiers and keywords. However, for quote and quasiquote constructors, they gain meaning in separating arguments. 

Parentheses are either `(` and `)`, or `[` and `]`, or any combination of one from each group. Besides `()`, which represents the empty list, a pair of parentheses is an s-expression. This represents all clauses, operations, expressions, and other things that are the atoms of Poplang programs.

Line comments begin with `;` or `;;` and tell the lexer to ignore all character data until reaching a newline. 

Octothorpes (`#`) serve as a means of declaring raw data more expressively, and will be used for representing hexadecimal, character, and other integer data better in source, as well as the basis for declaring new flags besides booleans. Currently, they serve only to create the literal boolean values for true and false, `#t` and `#f`, respectively.

`fun` and `call` are reserved keywords which *should* have no functionality currently, and should produce unknown compiler behaviour. 

Definitions: `define` is used for creating globals or functions, where the first argument is either an identifier (which creates a global) or an s-expression containing a variable number of identifiers (at least one), where the first identifier is the name of the function, and proceeding identifiers are the names of the parameters. Regardless, the second and final argument to `define` must be an expression. `define` only works at the global scope, however, and should not be used outside of this. Additionally, `define` is the only expression that does not return a value.

Finally, the language has one more trick up its sleeve: the gereed. Gereeds are an octothorpe followed by a semicolon (`#;`), and when used at the global scope, supress the runtime from printing the returned value of the expression. When used in local scopes, it supresses the preceeding expression from returning its value, and instead evaluates the following expression and returns that value instead. This allows for limited imperative programming.

# Roadmap

With most of the bugs being ironed out after the initial release, the focus is to next implement Boxes properly and fix all side-effects produced by non-set operations. Additionally, escape analysis tools will need to be implemented to compile the new Box implementation properly. 

After this, the main goal is to reduce the significant bloat code generation has by doing primitive optimizations and improving framing. 

After that, the final goal our team has in the near future is to allow for 32-bit compilation.