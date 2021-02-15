START equ $c000
A equ #5

org START
boucle
lda A
ldb #1
bra boucle
lda A
