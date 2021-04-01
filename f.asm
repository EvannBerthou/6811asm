START equ $c150

    org START
    lds >$FFFF
    ldb #%1000
boucle
    lda #1
    bra boucle
