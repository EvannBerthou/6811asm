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
    union {
        struct {
            uint8_t a;
            uint8_t b;
        };
        uint16_t d;
    };
    uint16_t sp;
    uint16_t pc;
    uint8_t status;

    uint8_t ram[MAX_RAM];
} cpu;

const char *register_name(register_type rt) {
    switch (rt) {
    case ACC_A: return "ACC A";
    case ACC_B: return "ACC B";
    case ACC_D: return "ACC D";
    default: return "Undefined";
    }
}

uint16_t read_register(cpu *cpu, register_type rt) {
    uint16_t value = 0;
    switch (rt) {
    case ACC_A: value = cpu->a; break;
    case ACC_B: value = cpu->b; break;
    case ACC_D: value = cpu->d; break;
    default: break;
    }
    printf("Reading %s: %04x\n", register_name(rt), value);
    return value;
}

void write_register(cpu *cpu, register_type rt, uint16_t value) {
    // Only keep the highest and lowest 8 bits for acc_a and acc_b respectively.
    // Keep the value as it is for acc_d.
    printf("Writing %s: %04x\n", register_name(rt), value);
    switch (rt) {
    case ACC_A: cpu->a = value; break;
    case ACC_B: cpu->b = value; break;
    case ACC_D: cpu->d = value; break;
    default: break;
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
    write_register(&cpu, ACC_A, 0x33);
    write_register(&cpu, ACC_B, 0x66);
    read_register(&cpu, ACC_A);
    read_register(&cpu, ACC_B);
    read_register(&cpu, ACC_D);
    write_register(&cpu, ACC_A, 0x36);
    read_register(&cpu, ACC_D);

    printf("\nMemory\n");
    write_memory(&cpu, 0x0, 0x38);
    read_memory(&cpu, 0x0);

    write_memory(&cpu, 0xFFFF, 0x87);
    read_memory(&cpu, 0xFFFF);
}

