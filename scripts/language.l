;;lang pop

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
     (match ls
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
     (match ls
        [() 1]
        [(cons n lt) 
          (match (interp n)
            [(? integer? i1)  
             (match (interp `(prod-e ,lt))
               [(? integer? i2) (* i1 i2)]
               [() 1]
               [_ 'err])]
            [_ 'err])])]))