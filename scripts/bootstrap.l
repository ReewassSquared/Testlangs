;; '(Listof Symbol) -> Bool
(define (asm-write asm) 
    (match ,asm
      	[(? integer? i) i]))

;; Bool String -> Nil
(define (assert c s)
	(if (not (boolean? c)) #f
		(if c
			#t
			(print s)#; 
			#f)))

(define (gensym-sysintern s)
	(let 
		((f0 
			(match (mod s 10)
				[0 'p]
				[1 'o]
				[2 'l]
				[3 'a]
				[4 'r]
				[5 'i]
				[6 'n]
				[7 's]
				[8 'm]
				[9 'w])))
		(if (<= s 9) f0 (symbol-append (gensym-sysintern (/ s 10)) f0))))

(define gensym-counter 1)

(define (gensym)
	(let ((sym (gensym-sysintern gensym-counter)))
		(set! gensym-counter (add1 gensym-counter))#;
		sym))

(define (gensym-pre pre)
	(let ((sym (symbol-append pre (gensym-sysintern gensym-counter))))
		(set! gensym-counter (add1 gensym-counter))#;
		sym))

(gensym)
(gensym)
(gensym)
(gensym)
(gensym)
(gensym-pre 'mayisthebestmonthoftheyear)
(define (for? x f)
	(if 
		(<= x 1)
		(f) 
		(f)#; 
		(for? (sub1 x) f)))

(for? 1048576 gensym)

(assert (= 1 2) "1 != 2")
;; this source file contains example code
;; lots of fun example code

(define (make-expr s)
  (match ,s
    [(list 'if q t f) 
      `(if-e ,(make-expr q) ,(make-expr t) ,(make-expr f))]
    [(list 'add1 e) `(add1-e ,(make-expr e))]
    [(list 'sub1 e) `(sub1-e ,(make-expr e))]
    [(list 'zero? e) `(zero?-e ,(make-expr e))]
    [(? integer? i) `(int-e ,i)]
    [(? boolean? b) `(bool-e ,b)]
    [es
        (match (car es)
            ['+ `(sum-e ,(make-var-expr (cdr es)))]
            ['* `(prod-e ,(make-var-expr (cdr es)))])]))

(define (make-var-expr es)
  (match es
    [() ()]
    [(cons e et) (cons (make-expr e) (make-var-expr et))]))

(define (interp e)
  (match ,e
    [(list 'int-e ,i) i]
    [(list 'bool-e ,b) b]
    [(list 'add1-e e0)
     (match (interp e0)
       [(? integer? i) (add1 i)]
       [_ 'err])]
    [(list 'sub1-e e0)
     (match (interp e0)
       [(? integer? i) (sub1 i)]
       [_ 'err])]
    [(list 'zero?-e e0)
     (match (interp e0)
       [(? integer? i) (zero? i)]
       [_ 'err])]
    [(list 'if-e e0 e1 e2)
     (match (interp e0)
       ['err 'err]
       [v
        (if v
            (interp e1)
            (interp e2))])]
    [(list 'sum-e ls)
     (match ,ls
        [() 0]
        [(cons n lt) 
          (match (interp n)
            [(? integer? i1)
             (match (interp `(sum-e ,lt))
               [(? integer? i2) (+ i1 i2)]
               [() 0]
               [_ 'err])]
            [_ 'err])])]
    [(list 'prod-e ls)
     (match ,ls
        [() 1]
        [(cons n lt) 
          (match (interp n)
            [(? integer? i1)  
             (match (interp `(prod-e ,lt))
               [(? integer? i2) (* i1 i2)]
               [() 1]
               [_ 'err])]
            [_ 'err])])]))

(interp (make-expr '(if #t (+ 1 2 3 4) 2)))

(define (nword a b c d) 
     (cond (
        [(= b 0) d]
        [(<= a 1) (if (= b 1) d (nword (* c c) (sub1 b) (* c c) d))]
        [else (nword (sub1 a) b c (add1 d))])))
(nword 0 4 10 0)