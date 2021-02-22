START equ $c000
PORTA equ $1000
DDRA  equ $1001
PORTB equ $1004


org START
lda #5
ldb #10
sec
clc
sev
aba
tba
tab
aba
