;; lang poplang

;; type Expr =
;; | Integer
;; | Boolean
;; | Variable 
;; | PrimOp Expr
;; | SpecOp (Listof Expr)
;; | if Expr Expr Expr
;; | cond (Listof Clause) Expr
;; | let (Listof Binding) Expr

;; type PrimOp =
;; | add1 | sub1 | zero? | abs | neg

;; type SpecOp = 
;; | + | *

;; type Clause = Expr Expr
;; type Binding = Variable Expr

;; Symbol -> Expr
(define (make-expr e)
	(match ,e
		[(? integer? i) `(int-e ,i)]
		[(? boolean? b) `(bool-e ,b)]
		[(? symbol? s) `(var-e ,s)]
		[(list 'add1 e) `(add1-e ,(make-expr e))]
		[(list 'sub1 e) `(sub1-e ,(make-expr e))]
		[(list 'zero? e) `(zero?-e ,(make-expr e))]
		[(list 'abs e) `(abs-e ,(make-expr e))]
		[(list 'let ,bs e) `(let-e ,(make-bindings bs) ,(make-expr e))]
		[(list 'if q t f) `(if-e ,(make-expr q) ,(make-expr t) ,(make-expr f))]
		[(list 'cond ,cs) (make-clauses cs ())]
		[es
			(match (car es)
				['+ `(sum-e ,(make-var-expr (cdr es)))]
				['* `(prod-e ,(make-var-expr (cdr es)))]
				['- 
					(match (cdr es)
						[(list arg) `(neg-e arg)]
						[_ `(minus-e ,(make-var-expr (cdr es)))])]
				[_ `(err-e syntax-error)])]))

;; (Listof Symbol) -> (Listof Binding)
(define (make-bindings bs)
	(match bs
		[() ()]
		[(cons ,b bt) (cons (make-binding b) (make-bindings bt))]
		[_ `(err-e syntax-let-error)]))

;; (Listof Symbol) -> Binding
(define (make-binding b)
	(match b
		[(list v e) `(bind-e ,v ,(make-expr e))]
		[_ `(err-e syntax-binding-error)]))

;; (Listof Symbol) (Listof Clause) -> Expr
(define (make-clauses cs ce)
	(match cs
		[() ()]
		[(cons q ct) 
			(match ,q
				[(list 'else e) `(cond-e ce ,(make-expr e))]
				[_ (make-clauses ct (cons (make-clause q) ce))])]
		[_ `(err-e syntax-cond-error)]))

;; Symbol -> Clause
(define (make-clause c)
	(match ,c
		[(list q a) `(clause ,(make-expr q) ,(make-expr a))]
		[_ `(err-e syntax-cond-clause-error)]))

;; (Listof Symbol) -> (Listof Expr)
(define (make-var-expr es)
  (match es
    [() ()]
    [(cons e et) (cons (make-expr e) (make-var-expr et))]))

(define (interp e) (interp-eval e ()))

(define (interp-eval e env)
	(match ,e
		[(list 'int-e i) ,i]
		[(list 'var-e v) (lookup-env v env)]
		[(list 'bool-e b) ,b]
		[(list 'err-e s) s]
		[(list 'add1-e e) 
			(match (interp-eval e env)
				[(? integer? i) (add1 i)]
				[(? symbol? s) s]
				[_ 'runtime-error-add1])]
		[(list 'sub1-e e) 
			(match (interp-eval e env)
				[(? integer? i) (sub1 i)]
				[(? symbol? s) s]
				[_ 'runtime-error-sub1])]
		[(list 'zero?-e e) 
			(match (interp-eval e env)
				[(? integer? i) (zero? i)]
				[(? symbol? s) s]
				[_ 'runtime-error-zero?])]
		[(list 'abs-e e)
			(match (interp-eval e env)
				[(? integer? i) (abs i)]
				[(? symbol? s) s]
				[_ 'runtime-error-abs])]
		[(list 'neg-e e)
			(match (interp-eval e env)
				[(? integer? i) (- i)]
				[(? symbol? s) s]
				[_ 'runtime-error-neg])]
		[(list 'if-e q t f)
			(match (interp-eval q env)
				[#t (interp-eval t env)]
				[(? symbol? s) s]
				[_ (interp-eval f env)])]
		[(list 'let-e ,bs e)
			(match (build-env bs env env)
				[(? symbol? s) s]
				[nenv (interp-eval e nenv)])]
		[(list 'sum-e ls)
     (match ,ls
        [() 0]
        [(cons n lt) 
          (match (interp-eval n env)
						[(? symbol? s) s]
            [(? integer? i1)
             (match (interp-eval `(sum-e ,lt) env)
						 	 [(? symbol? s) s]
               [(? integer? i2) (+ i1 i2)]
               [() 0]
               [_ 'runtime-err-sum])]
            [_ 'runtime-err-sum])])]
    [(list 'prod-e ls)
     (match ,ls
        [() 1]
        [(cons n lt) 
          (match (interp-eval n env)
						[(? symbol? s) s]
            [(? integer? i1)  
             (match (interp-eval `(prod-e ,lt) env)
						 	 [(? symbol? s) s] 
               [(? integer? i2) (* i1 i2)]
               [() 1]
               [_ 'runtime-err-prod])]
            [_ 'runtime-err-prod])])]))

(define (build-env bs oenv env)
	(match bs
		[() env]
		[(list 'err-e 'syntax-let-error) 'syntax-let-error]
		[(cons b bt) 
			(match ,b
				[(? symbol? s) s]
				[(list 'bind-e v e)
					(match (interp-eval e oenv)
						[(? symbol? s) s]
						[val 
							(build-env bt oenv (cons (cons v val) env))])])]))

(define (lookup-env v env)
	(match env
		[() (symbol-append 'runtime-err-notfound- v)]
		[(cons (cons vn val) envt)
			(if (symbol=? vn v)
				val
				(lookup-env v envt))]))

(make-expr '(let ((x 7)) x))
(interp-eval '(var-e x) (list (cons 'x 7)))
(build-env (list '(bind-e x (int-e 7))) () ())
(interp (make-expr '(let ((x 7)) x)))
(interp (make-expr '(let ((#t (if (zero? 0) 1 2)) (y 2)) (+ x #t))))
(make-expr '(let ((#t (if (zero? 0) 1 2)) (y 2)) (+ x #t)))