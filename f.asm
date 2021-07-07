START equ $C015

    org START
    lda #%10101
    sta $10
    bclr <$10 $e ; 1110 => 10001 = 17 = 0x11
    lda <$10
    lda #02
    lda #02
    lda #02
    lda #02
    lda #02
    lda #02
    lda #02
    lda #02
    lda #02
    lda #02
    lda #02
    asra
