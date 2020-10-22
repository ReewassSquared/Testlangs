;;(+ 1 1)
(define (whatthefuck) (+ 1 2 3 4))
;;(whatthefuck)

(define (stack-driver a b c) 
  (if (zero? c) 0 
    (+ a (stack-driver a b (sub1 c)) b)))

(define (single-arg a) a)
;;(single-arg 1)

;;(stack-driver 4 4 20)

;;(interp (make-expr '(add1 7)))
;;(interp (make-expr '(if (zero? 0) 1 2)))
;;(interp (make-expr '(+ 1 2 3)))
;;(interp (make-expr '(+ 1 2 3 4)))


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

(define (random-src cnt)
    (cond (
        [(zero? cnt) 1]
        [(zero? (mod cnt 2))
            `(+ ,cnt ,(random-src (sub1 cnt)) ,cnt ,(random-src (sub1 cnt)) ,cnt)]
        [else `(add1 ,(random-src (sub1 cnt)))]
    )))

(define (grow-list cnt)
  (cond (
    [(zero? cnt) 1]
    [else 
      (cons 
        (grow-list (sub1 cnt))
        (grow-list (sub1 cnt)))]
  )))

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

;;(make-expr (random-src 2))

(define (test-gc cnt dummy)
  (cond (
    [(zero? cnt) 0]
    [else (test-gc (sub1 cnt) (random-src 2))]
  )))

(test-gc 3000 0)

(define myglobal 2)
myglobal

(define bestglobal myglobal)

(set! bestglobal 3)#;
bestglobal
myglobal

(set! bestglobal )



(define (test-imperative c g)
  (set! g (add1 g))#;
  (if (zero? c) g (test-imperative (sub1 c) g)))

(test-imperative 10 0)

(define (file-test)
  (let ((w0 (file-open "my-file-test.txt")))
    (if w0 
      (let ((w1 (file-write "test\n")))
        (if w1
          (let ((w2 (file-close)))
            (if w2
              "all tests passed successfully."
              "file error: could not close."
            )
          )
          "file error: could not write."
        )
      )
      "file error: could not open."
    )))

(file-test)



;;(interp (make-expr (random-src 12)))
;;((let ((x 8)) (lambda (y) x)) 2)
;;(random-src 10)
;;(random-src 18)#;
;;(random-src 18)#;
;;(random-src 18)#;
;;(let ((dumbassbitch  (random-src 20)))
;;(interp (make-expr dumbassbitch)))
;; it is now possible to 
;; the above might not work
;;lang pop
;;next we are going to add:
;; (file-open "filename")
;; (file-write data)
;; (file-close)