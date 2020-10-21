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
				[0 'g0]
				[1 'g1]
				[2 'g2]
				[3 'g3]
				[4 'g4]
				[5 'g5]
				[6 'g6]
				[7 'g7]
				[8 'g8]
				[9 'g9]))) 
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

(for? 1000000 gensym)

(assert (= 1 2) "1 != 2")