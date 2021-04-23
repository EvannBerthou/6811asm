START equ $C015

    org START
    lda #10
    sta <$0
    ldb #5
    stb <$1
    lda #0
    ldb #0

boucle
    bra boucle
