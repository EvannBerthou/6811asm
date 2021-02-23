START equ $c150
PORTA equ $1000
DDRA  equ $1001
PORTB equ $1004

org START
lda #10
cmpa #10
beq eq

paseq
nop
bra paseq

eq
lda #FF
bra eq
