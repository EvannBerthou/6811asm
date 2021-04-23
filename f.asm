START equ $c150

    org START
    lds #$FFFF
    lda #$01
    inca
    jsr f
    ldb #22
    addb $50

f
    psha
    lda #$00
    lda #$00
    lda #$00
    lda #$00
    pula
    rts
