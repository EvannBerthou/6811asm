START equ $C015

    org START
    lda #%1001
    oraa #%1010
; Gives #%1011 => 11 = 0xB


boucle
    bra boucle
