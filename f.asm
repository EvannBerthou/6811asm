START equ $C000

org START
lda #6
ldb #5
aba
sta $0
lda #6
ldb $0
aba
