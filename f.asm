START equ $c150

    org START
    lds #$FFFF
    lda #$FF
    jsr f
    ldb #15
boucle
    bra boucle

f
    lda #$00
    rts
