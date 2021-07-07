START equ $C015
TEST equ $10

    org START
    lda #%10101
    sta $10
    bclr <$10 TEST
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
