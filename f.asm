START equ $c150

    org START
    lds #FF
boucle
    lda #1
    bra boucle
