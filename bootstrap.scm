(define apply (lambda (fun args) (eval (cons fun args))))
(define map (lambda (fun lst) (if (null? lst) (quote ()) (cons (fun (car lst)) (map fun (cdr lst))))))
