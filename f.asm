START equ $c000
PORTA equ $1000
DDRA equ $1001
PORTE equ $100a

org START
lda #00
sta DDRA
lda #FF
sta PORTA
sta PORTE
