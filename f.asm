START equ $c150

    org START
    lds #$FFFF
    lda #$FF
    jsr f
    ldb #22
boucle
    bra boucle

f
    psha
    lda #$00
    lda #$00
    lda #$00
    lda #$00
    pula
    rts
