START equ $c150

        org START
        ldb #10
        bra petit
petit   lda #5
        lda #10
        bra petit
