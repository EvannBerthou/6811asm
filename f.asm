START equ $C015
    org $c015
    lds #$FFFF
    lda #$01
    inca
    jsr f
    ldb #22

f
    psha
    lda #$00
    lda #$00
    lda #$00
    lda #$00
    pula
    rts
