; basics functions
(define apply (lambda (fun args) (eval (cons fun args))))
(define map (lambda (fun lst) (if (null? lst) (quote ()) (cons (fun (car lst)) (map fun (cdr lst))))))
(define length (lambda (lst) (if (null? lst) 0 (+ 1 (length (cdr lst))))))
(define append (lambda (lst1 lst2) (if (null? lst1) lst2 (cons (car lst1) (append (cdr lst1) lst2)))))

(define foldr
    (lambda (fun terminator lst)
        (if
            (null? lst)
            terminator
            (fun (car lst) (foldr fun terminator (cdr lst)))
        )
    )
)

(define foldl
    (lambda (fun terminator lst)
        (if
            (null? lst)
            terminator
            (foldl fun (fun (car lst) terminator) (cdr lst))
        )
    )
)

(define reverse (lambda (lst) (foldl cons (quote ()) lst)))

; boolean operations
(define not (lambda (x) (if (null? x) #t #f)))
(define and2 (lambda (a b) (if a b #f)))
(define or2
    (lambda (a b) (if a #t b)))
