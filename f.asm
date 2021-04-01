START equ $c150

    org START
    lds #$FFFFF
boucle
    lda #1
    bra boucle
