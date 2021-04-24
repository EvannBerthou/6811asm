START equ $C015

    org START
    lda #$05
    ldb #$02
    sta $0
    stb $1
    lda #$20
    ldb #$10
    subd <$0
    clra
; Gives 0x2010 - 0x0502 = 0x1B0E


boucle
    bra boucle
