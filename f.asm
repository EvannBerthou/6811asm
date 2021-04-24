START equ $C015

    org START
    lda #5
    suba #2
; Gives #3


boucle
    bra boucle
