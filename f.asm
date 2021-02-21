START equ $c000
PORTA equ $1000
DDRA  equ $1001
PORTB equ $1004

org START
lda #5
sta $E0F0
ldb $E0F0
cmpb #1
