START equ $C015

    org START
    lda #$03
    ldb #$02
    mul
; Gives 0x3 * 0x2 = 0x6

