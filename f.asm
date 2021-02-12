START equ $C000

org START
lda #1
ldb #1
aba
sta $0
ldb $0
aba
