#define EMULATOR_IMPLEMENTATION
#include "../src/emulator.h"
#include "tests.h"

const char *fail_fmt = FMT8" != "FMT8"\n";

void exec_instr(cpu *cpu, int opcode) {
    (*instr_func[opcode])(cpu);
}

int cmp_mnemonic(mnemonic *m1, mnemonic *m2) {
    int err = 1;
    if (m1->opcode != m2->opcode) {
        FAIL(fail_fmt, m1->opcode, m2->opcode);
        err = 0;
    }

    if (m1->operand.type != m2->operand.type) {
        FAIL(fail_fmt, m1->operand.type, m2->operand.type);
        err = 0;
    }

    if (m1->operand.value != m2->operand.value) {
        FAIL(fail_fmt, m1->operand.value, m2->operand.value);
        err = 0;
    }
    return err;
}

mnemonic new_mnemonic(u8 opcode, u16 operand_value, operand_type type, u8 immediate_16) {
    return (mnemonic){opcode, {operand_value, type, 0}, immediate_16};
}

void strcopy(char **s, const char *dup) {
    if (*s != NULL) {
        free(*s);
    }

    *s = malloc(strlen(dup) + 1);
    if (*s == NULL) {
        exit(1);
    }
    memcpy(*s, dup, strlen(dup) + 1);
}

int main() {
    cpu cpu = {0};
    add_instructions_func();

    TEST ("Mneominic parsing") {
        char *s = NULL;
        mnemonic m, e;

        //LDA
        strcopy(&s, " lda #$FF");
        m = line_to_mnemonic(s, NULL, 0, 0);
        e = new_mnemonic(0x86, 0xFF, IMMEDIATE, 0);
        ASSERT(cmp_mnemonic(&m, &e));

        strcopy(&s, " lda <$FF");
        m = line_to_mnemonic(s, NULL, 0, 0);
        e = new_mnemonic(0x96, 0xFF, DIRECT, 0);
        ASSERT(cmp_mnemonic(&m, &e));

        strcopy(&s, " lda $FF");
        m = line_to_mnemonic(s, NULL, 0, 0);
        e = new_mnemonic(0xB6, 0xFF, EXTENDED, 0);
        ASSERT(cmp_mnemonic(&m, &e));

        //LDB
        strcopy(&s, " ldb #$FF");
        m = line_to_mnemonic(s, NULL, 0, 0);
        e = new_mnemonic(0xC6, 0xFF, IMMEDIATE, 0);
        ASSERT(cmp_mnemonic(&m, &e));

        strcopy(&s, " ldb <$FF");
        m = line_to_mnemonic(s, NULL, 0, 0);
        e = new_mnemonic(0xD6, 0xFF, DIRECT, 0);
        ASSERT(cmp_mnemonic(&m, &e));

        strcopy(&s, " ldb $FF");
        m = line_to_mnemonic(s, NULL, 0, 0);
        e = new_mnemonic(0xF6, 0xFF, EXTENDED, 0);
        ASSERT(cmp_mnemonic(&m, &e));

        //LDD
        strcopy(&s, " ldd #$FFFF");
        m = line_to_mnemonic(s, NULL, 0, 0);
        e = new_mnemonic(0xCC, 0xFFFF, IMMEDIATE, 0);
        ASSERT(cmp_mnemonic(&m, &e));

        strcopy(&s, " ldd <$FF");
        m = line_to_mnemonic(s, NULL, 0, 0);
        e = new_mnemonic(0xDC, 0xFF, DIRECT, 0);
        ASSERT(cmp_mnemonic(&m, &e));

        strcopy(&s, " ldd $FF");
        m = line_to_mnemonic(s, NULL, 0, 0);
        e = new_mnemonic(0xFC, 0xFF, EXTENDED, 0);
        ASSERT(cmp_mnemonic(&m, &e));

        //STA
        strcopy(&s, " ldd <$FF");
        m = line_to_mnemonic(s, NULL, 0, 0);
        e = new_mnemonic(0xDC, 0xFF, DIRECT, 0);
        ASSERT(cmp_mnemonic(&m, &e));

        strcopy(&s, " ldd $FF");
        m = line_to_mnemonic(s, NULL, 0, 0);
        e = new_mnemonic(0xFC, 0xFF, EXTENDED, 0);
        ASSERT(cmp_mnemonic(&m, &e));
    }

    TEST ("Shift tests tests") {
        cpu.a = 0xFF;
        mnemonic m = line_to_mnemonic((char[]){" lsla"}, NULL, 0, 0);
        exec_instr(&cpu, m.opcode);
        ASSERT_EQ(cpu.a, ((0xFF << 1) & 0xFF));
        ASSERT_EQ(cpu.c, 1);

        cpu.d = 0xFFF0;
        m = line_to_mnemonic((char[]){" lsrd"}, NULL, 0, 0);
        exec_instr(&cpu, m.opcode);
        ASSERT_EQ(cpu.d, (0xFFF0 >> 1));
        ASSERT_EQ(cpu.c, 0);

        cpu.c = 1;
        cpu.a = 0x7F;
        m = line_to_mnemonic((char[]){" rora"}, NULL, 0, 0);
        exec_instr(&cpu, m.opcode);
        ASSERT_EQ(cpu.a, (0x7F >> 1) | (cpu.c << 7));
        ASSERT_EQ(cpu.c, 1);

        cpu.c = 1;
        cpu.a = 0x0;
        m = line_to_mnemonic((char[]){" rola"}, NULL, 0, 0);
        exec_instr(&cpu, m.opcode);
        ASSERT_EQ(cpu.a, 1);
        ASSERT_EQ(cpu.c, 0);
    }

    return 0;
}
