START equ $C015

    org START
    lda #$05
    coma
; Gives 0xFF - 0x5 = 0xFA


boucle
    bra boucle
