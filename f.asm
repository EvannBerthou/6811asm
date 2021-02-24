START equ $c150

        org START
        ldb #10
petit   lda #5
        aba
        bra petit
