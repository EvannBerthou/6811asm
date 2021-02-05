#include <stdio.h>
#include <stdint.h>

#define MAX_RAM (1 << 16)

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

    uint8_t ram[MAX_RAM];
} cpu;

uint16_t read_register(cpu *cpu, register_type rt) {
    switch(rt) {
    case ACC_A: return cpu->d & 0xFF00;
    case ACC_B: return cpu->d & 0x00FF;
    case ACC_D: return cpu->d;
    default: return 0;
    }
}

void write_register(cpu *cpu, register_type rt, uint16_t value) {
    // Only keep the highest and lowest 8 bits for acc_a and acc_b respectively.
    // Keep the value as it is for acc_d.
    switch(rt) {
    case ACC_A: cpu->d = (cpu->d & 0x00FF) | (value << 8); break;
    case ACC_B: cpu->d = (cpu->d & 0xFF00) | (value & 0x00FF) ; break;
    case ACC_D: cpu->d = value; break;
    default: return;
    }
}

void write_memory(cpu *cpu, uint16_t pos, uint8_t value) {
    printf("Writing at %04x: %04x\n", pos, value);
    cpu->ram[pos] = value;
}

uint8_t read_memory(cpu *cpu, uint16_t pos) {
    printf("Reading at %04x: %04x\n", pos, cpu->ram[pos]);
    return cpu->ram[pos];
}


int main() {
    cpu cpu = {0};

    printf("Registers\n");
    write_register(&cpu, ACC_B, 0x66);
    write_register(&cpu, ACC_A, 0x33);
    printf("%04x\n", read_register(&cpu, ACC_A));
    printf("%04x\n", read_register(&cpu, ACC_B));
    printf("%04x\n", read_register(&cpu, ACC_D));
    write_register(&cpu, ACC_A, 0x36);
    printf("%04x\n", read_register(&cpu, ACC_D));

    printf("\nMemory\n");
    write_memory(&cpu, 0x0, 0x38);
    read_memory(&cpu, 0x0);

    write_memory(&cpu, 0xFFFF, 0x87);
    read_memory(&cpu, 0xFFFF);
}

