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
    uint8_t status;

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

void write_register(cpu *cpu, register_type rt, uint16_t value) {
    switch(rt) {
    case ACC_A: cpu->d = (cpu->d & 0x00FF) | (value << 8); break;
    case ACC_B: cpu->d = (cpu->d & 0xFF00) | (value & 0x00FF) ; break;
    case ACC_D: cpu->d = value; break;
    default: return;
    }
}

int main() {
    cpu cpu = {0};
    write_register(&cpu, ACC_B, 0x66);
    write_register(&cpu, ACC_A, 0x33);
    printf("%04x\n", get_register_value(&cpu, ACC_A));
    printf("%04x\n", get_register_value(&cpu, ACC_B));
    printf("%04x\n", get_register_value(&cpu, ACC_D));
    write_register(&cpu, ACC_A, 0x36);
    printf("%04x\n", get_register_value(&cpu, ACC_D));
}
