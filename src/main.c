#include <stdio.h>
#include <stdint.h>

typedef enum {
    ACC_A,
    ACC_B,
    ACC_D,
    ACC_X,
    ACC_Y,
    SP,
    PC
} register_type;

typedef struct {
    uint16_t d;
    uint16_t sp;
    uint16_t pc;

    uint8_t ram[(1 << 16)];
} cpu;

uint16_t get_register_value(cpu *cpu, register_type rt) {
    switch(rt) {
    case ACC_A: return cpu->d & 0xFF00;
    case ACC_B: return cpu->d & 0x00FF;
    case ACC_D: return cpu->d;
    default: return 0;
    }
}

int main() {
    cpu cpu = {0};
    cpu.d = 0xFF11;
    printf("%04x\n", get_register_value(&cpu, ACC_A));
    printf("%04x\n", get_register_value(&cpu, ACC_B));
    printf("%04x\n", get_register_value(&cpu, ACC_D));
}
