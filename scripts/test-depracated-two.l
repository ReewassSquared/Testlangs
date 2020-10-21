'(() test (foo bar ()))
`(quasiquote is cool!)
'7
'(7 8 "magic")
'(() foo bar "baz" . ("baz2" . 27))
`,7
`(,7)
,'7
(cons ,(car ,'(1 2)) (cons ,(car (cdr ,'(1 2))) ,'()))
(length ,'(a b c))
(list)
(list 1)
(list 1 2)
(list 1 2 3 4 5 6 7 8 9 10 11)
(not #t)
(not #f)
(cdr (list 1 2 3))
(define (even? x) 
    (let ((modx2 (mod x 2)))
        (if (zero? modx2) #t #f)
    ))
(define (odd? x) 
    (not (even? x)))
(define (not-integer? x) (not (integer? x)))
(define (parity? v)
    (match v
        [(? not-integer? x) 'err]
        [(? even? x) 'even]
        [(? odd? x) 'odd]
        [_ 'err]))
(define (list-length? v) (list-length?-proto v 0))
(define (list-length?-proto v x)
    (match v
        [() x]
        [(cons h t) (list-length?-proto t (add1 x))]
        [_ 'err]))
(parity? ,'7)
(list-length? (list 1 2 3 4 5 6))
(define (btree-sum bt)
    (match ,bt
        ['leaf 0]
        [(list 'node ,v l r)
            (+ v (btree-sum l) (btree-sum r))]))
(btree-sum '(node 1 
                (node 2 
                    (node 3 leaf leaf) 
                    (node 4 leaf 
                        (node 5 leaf 
                            (node 6 
                                (node 7 leaf leaf) 
                                (node 8 leaf leaf)
                            )
                        )
                    )
                ) 
                leaf
            )
)
1#;
`(,(lambda (x) x) fuckmeintheass)