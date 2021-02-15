START equ $c000
PORTA equ $1000
DDRA  equ $1001

org START
lda #FF
sta DDRA
lda #FF
sta PORTA
