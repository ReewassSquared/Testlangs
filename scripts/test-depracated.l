(add1 1)
(define (inc x) (+ x x))
(define (dec x) (+ x x x))
(define (magic2 y) (if (= y 0) inc dec))
((magic2 1) 5)
(define (identity x) x)
(identity 16384)
(let 
    (
        (x 5) 
        (y (let 
            (
                (z 1)
                (w 2)
            )
            (cond
                (
                    [(zero? z) w]
                    [(zero? (- w 2)) 
                        (+ (let ((z 100)) z) z)]
                    [else -1]
                )
            )
            ))
    )
    (if (zero? 1) (+ x y) (cons 1 (cons 2 (cons #f -16384)))))
(define (nword a b c d) 
     (cond (
        [(= b 0) d]
        [(<= a 1) (if (= b 1) d (nword (* c c) (sub1 b) (* c c) d))]
        [else (nword (sub1 a) b c (add1 d))])))
(nword 0 4 7 0)
(= 1 1)
(define (double x) (cons #t (cons #f x)))
(define (triple x) (cons (box x) (box 7)))
(define (magic y) (cond ([(= y 0) double] [else triple])))
((magic 0) 5)
(triple 2)
((let ((x 8)) (lambda (y) x)) 2)
((lambda (f) (f (f 0))) (lambda (x) (add1 x)))
(((lambda (t)
         ((lambda (f) (t (lambda (z) ((f f) z))))
          (lambda (f) (t (lambda (z) ((f f) z))))))
       (lambda (tri)
         (lambda (n)
           (if (zero? n)
               1
               (+ n (tri (sub1 n)))))))
      10)
(define (Y) (lambda (t)
         ((lambda (f) (t (lambda (z) ((f f) z))))
          (lambda (f) (t (lambda (z) ((f f) z)))))))
(define (tris) (lambda (tri)
               (lambda (n)
                 (if (zero? n)
                     1
                     (+ n (tri (sub1 n)))))))
(define (list-test y) (let ((x (cons 1 2))) (if (zero? y) y (list-test (sub1 y)))))
(((Y) (tris)) 1)
(((Y) (tris)) 2000)
(list-test 4345678)
(let ((x 5) (y 7) (f (cons 2 3)) (g (cons 1 (cons 2 (cons 3 ()))))) 
    (lambda (z) 
        (if (zero? x) f (if (zero? y) g z))
    ))
list-test
"test"
""
"longerstring"
"a string that is so long that it would need\na line break in it to print neatly"
'test 


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
       [(? integer? ii) (+ ii 1)]
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

;; tomorrow we will add:
;; functions (done)
;; first-class function pointers (done)
;; and, or, strings, len, 