START equ $c000
PORTA equ $1000
DDRA  equ $1001
PORTB equ $1004


petit
lda #1
bra petit

org START
lda #5
cmpa #12
blo petit
lda #10

