START equ $C015

    org START
    lda #1
    sta $FF
    als $FF
    als $FF
    als $FF
    als $FF
    lda $FF


boucle
    bra boucle
