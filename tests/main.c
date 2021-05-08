#define EMULATOR_IMPLEMENTATION
#include "../src/emulator.h"
#include "tests.h"

void exec_instr(cpu *cpu, int opcode) {
    (*instr_func[opcode])(cpu);
}

int cmp_mnemonic(mnemonic *m1, mnemonic *m2) {
    return (m1->opcode == m2->opcode)
           && (m1->operand.type == m2->operand.type)
           && (m1->operand.value == m2->operand.value);
}

int main() {
    cpu cpu = {0};
    add_instructions_func();

    /*
    TEST ("Mneominic parsing") {
        char s1[] = " lda #$FF";
        mnemonic m1 = line_to_mnemonic(s1, NULL, 0, 0);
        mnemonic e1 = {0x86, {0xFF, IMMEDIATE, 0}, 0};
        ASSERT(cmp_mnemonic(&m1, &e1));

        char s2[] = " lda $FF";
        mnemonic m2 = line_to_mnemonic(s2, NULL, 0, 0);
        mnemonic e2 = {0xB6, {0xFF, EXTENDED, 0}, 0};
        ASSERT(cmp_mnemonic(&m2, &e2));

        char s3[] = " lda <$FF";
        mnemonic m3 = line_to_mnemonic(s3, NULL, 0, 0);
        mnemonic e3 = {0x96, {0xFF, DIRECT, 0}, 0};
        ASSERT(cmp_mnemonic(&m3, &e3));

        char s4[] = " lda <$FF";
        mnemonic m4 = line_to_mnemonic(s4, NULL, 0, 0);
        mnemonic e4 = {0x96, {0xFF, EXTENDED, 0}};
        ASSERT(!cmp_mnemonic(&m4, &e4));

        char s5[] = " rts";
        mnemonic m5 = line_to_mnemonic(s5, NULL, 0, 0);
        mnemonic e5 = {0x39, {0, INHERENT, 0}};
        ASSERT(cmp_mnemonic(&m5, &e5));
    }*/

    TEST ("LSL and LSR tests") {
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
    }

    return 0;
}
