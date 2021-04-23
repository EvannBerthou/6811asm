START equ $C015

    org START
    lda #$10
    sta $FF
    neg $FF
    lda $FF
    nega


boucle
    bra boucle
