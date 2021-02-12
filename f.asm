START equ $C000

org START
lda #6
ldb #5
aba
stb $0
lda $0
ldb #2
aba
