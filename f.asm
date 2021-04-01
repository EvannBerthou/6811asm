START equ $c150

    org START
    lds #$FFFF
    ldb $FF
boucle
    lda #1
    bra boucle
