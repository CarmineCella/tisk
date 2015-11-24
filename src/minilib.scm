(def a 32)
(def b 22)
(def double (fn (x) (+ x x)))
(quote loaded)

(def fib1 (fn (m)
  (if (< m 2)
      m
      (+ (fib1 (- m 1)) (fib1 (- m 2)))))
	 
(def fib (fn (n)
  (if (<= n 2)
      1
      (+ (fib (- n 1)) (fib (- n 2)))))