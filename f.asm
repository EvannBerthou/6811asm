START equ $c000
PORTA equ $1000
DDRA  equ $1001
PORTB equ $1004

org START
ldb #80
cmpb #1
