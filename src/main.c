#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define MAX_MEMORY (1 << 16)
#define FMT8 "0x%02x"
#define FMT16 "0x%04x"

typedef enum {
    IMMEDIATE,
    EXTENDED,
    DIRECT,
    INDEXDED,
    INHERENT,
    RELATIVE
} operand_type;

typedef struct {
    operand_type type;
    union {
        uint8_t as_u8;
        uint16_t as_u16;
    };
} operand;

typedef struct {
    const char *src;
    uint8_t code;
    operand operand;
} opcode;

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

    uint8_t memory[MAX_MEMORY];
} cpu;

void INST_LDA_IMM(cpu *cpu) {
    uint8_t value = cpu->memory[++cpu->pc]; // Operand is a constant, just load the next byte in memory
    printf("Loading " FMT8 " in ACC A\n", value);
    cpu->a = value;
}

void INST_LDA_DIR(cpu *cpu) {
    uint8_t addr = cpu->memory[++cpu->pc]; // Get value of the next operand
    printf("Loading value at address " FMT8 " (= " FMT8 ") in ACC A\n", addr, cpu->memory[addr]);
    cpu->a = cpu->memory[addr]; // Get value the operand is poiting at
}

void INST_LDA_EXT(cpu *cpu) {
    uint8_t b0 = cpu->memory[++cpu->pc]; // high order byte
    uint8_t b1 = cpu->memory[++cpu->pc]; // low order byte
    uint16_t addr = (b0 << 8) | b1;
    printf("Loading value at address " FMT16 " (= " FMT8 ") in ACC A\n", addr, cpu->memory[addr]);
    cpu->a = cpu->memory[addr];
}

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
    printf("Reading %s: " FMT16 "\n", register_name(rt), value);
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
    cpu->memory[pos] = value;
}

uint8_t read_memory(cpu *cpu, uint16_t pos) {
    printf("Reading at %04x: %04x\n", pos, cpu->memory[pos]);
    return cpu->memory[pos];
}

void print_memory_range(cpu *cpu, uint16_t from, uint16_t len) {
    for (uint16_t i = 0; i < len; ++i) {
        printf("%04x: %02x\n", i, cpu->memory[from + i]);
    }
}

void clear_memory(cpu *cpu) {
    int i = MAX_MEMORY;
    while (i--) {
        cpu->memory[i] = 0x0;
    }
}

void load_program(cpu *cpu) {
    const int org = 0xC000; // TODO: SHOULD BE DETERMINED IN THE CODE
    cpu->pc = org;
    cpu->memory[cpu->pc++] = 0x86;
    cpu->memory[cpu->pc++] = 0x30;
    cpu->memory[cpu->pc++] = 0x96;
    cpu->memory[cpu->pc++] = 0x0;
    cpu->pc = org;
}

void (*instr_func[0xFF]) (cpu *cpu) = {0};

void exec_program(cpu *cpu) {
    while (cpu->memory[cpu->pc] != 0x0) {
        uint8_t inst = cpu->memory[cpu->pc];
        if (instr_func[inst] != NULL) {
            (*instr_func[inst])(cpu); // Call the function with this opcode
        } else {
            printf(FMT8" is an undefined (or not implemented) opcode\n", inst);
        }
        cpu->pc++;
    }
}

int main() {
    instr_func[0x86] = INST_LDA_IMM;
    instr_func[0x96] = INST_LDA_DIR;
    instr_func[0xB6] = INST_LDA_EXT;
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

    print_memory_range(&cpu, 0xC000, 10);

    printf("\nLDAA IMM\n");
    write_memory(&cpu, 0xC001, 0x22);
    cpu.pc = 0xC000;
    INST_LDA_IMM(&cpu);
    printf("ACC A: %02x\n", cpu.a);

    printf("\nLDAA DIR\n");
    write_memory(&cpu, 0xC001, 0x00);
    write_memory(&cpu, 0x00, 0x37);
    cpu.pc = 0xC000;
    INST_LDA_DIR(&cpu);
    printf("ACC A: %02x\n", cpu.a);

    printf("\nLDAA EXT\n");
    write_memory(&cpu, 0xC001, 0x30);
    write_memory(&cpu, 0xC002, 0x56);
    write_memory(&cpu, 0x3056, 0xFF);
    cpu.pc = 0xC000;
    INST_LDA_EXT(&cpu);
    printf("ACC A: %02x\n", cpu.a);

    clear_memory(&cpu);

    printf("\nLoad Program\n");
    load_program(&cpu);
    write_memory(&cpu, 0x0, 0xFF);
    print_memory_range(&cpu, 0xC000, 10);

    printf("\nExec program\n");
    exec_program(&cpu);
}

