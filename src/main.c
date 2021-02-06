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
    const char *name;
    uint8_t codes;
    operand operands;
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
            uint8_t b; // B is low order
            uint8_t a; // A is high order
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

uint8_t part_count(const char *str) {
    uint8_t wc = 0;
    uint8_t state = 0;
    while (*str) {
        if (*str == ' ' || *str == '\n' || *str == '\t') {
            state = 0;
        } else if (state == 0) {
            state = 1;
            wc++;
        }
        str++;
    }
    return wc;
}

uint8_t instr_operand_count[0xFF] = {0};

uint8_t opcode_str_to_hex(const char *str) {
    (void) str;
    return 0x0;
}

opcode line_to_instruction(const char *line) {
    uint8_t nb_parts = part_count(line);
    if (nb_parts > 2) {
        printf("Too many operands\n");
        return (opcode) {0};
    }

    // Get instruction name
    char part[6] = {0}; // Longest instruction is BRCLR
    uint8_t part_len = 0;
    for (; line[part_len] != ' ' && line[part_len] != '\0' && part_len < 6; ++part_len) {
        part[part_len] = line[part_len];
    }
    if (part_len > 5) {
        printf("Instruction is too long\n");
    }

    uint8_t op = opcode_str_to_hex(part);

    uint8_t need_operand = instr_operand_count[op];
    if (need_operand != (nb_parts - 1)) { // need an operand but none were given
        printf("This instruction requires %d operand but %d recieved\n", need_operand, nb_parts - 1);
        return (opcode) {0};
    }

    return (opcode) {0};
}

void add_opcode_to_memory(cpu *cpu, opcode *op) {
    (void) cpu;
    (void) op;
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

void print_cpu_state(cpu *cpu) {
    printf("\n");
    printf("ACC A: "FMT8"\n", cpu->a);
    printf("ACC B: "FMT8"\n", cpu->b);
    printf("ACC D: "FMT16"\n", cpu->d);
    printf("SP: "FMT16"\n", cpu->sp);
    printf("PC: "FMT16"\n", cpu->pc);
    printf("Status : ");
    for (int i = 0; i < 8; ++i) {
        printf("%d", cpu->status >> i & 0x1);
    }
    printf("\n");

    printf("Next memory range\n");
    print_memory_range(cpu, cpu->pc, 10);
}

int main() {
    instr_func[0x86] = INST_LDA_IMM;
    instr_func[0x96] = INST_LDA_DIR;
    instr_func[0xB6] = INST_LDA_EXT;
    cpu c = {0};

    printf("Registers\n");
    write_register(&c, ACC_A, 0x33);
    write_register(&c, ACC_B, 0x66);
    read_register(&c, ACC_A);
    read_register(&c, ACC_B);
    read_register(&c, ACC_D);
    write_register(&c, ACC_A, 0x36);
    read_register(&c, ACC_D);

    printf("\nMemory\n");
    write_memory(&c, 0x0, 0x38);
    read_memory(&c, 0x0);

    write_memory(&c, 0xFFFF, 0x87);
    read_memory(&c, 0xFFFF);

    print_memory_range(&c, 0xC000, 10);

    printf("\nLDAA IMM\n");
    write_memory(&c, 0xC001, 0x22);
    c.pc = 0xC000;
    INST_LDA_IMM(&c);
    printf("ACC A: %02x\n", c.a);

    printf("\nLDAA DIR\n");
    write_memory(&c, 0xC001, 0x00);
    write_memory(&c, 0x00, 0x37);
    c.pc = 0xC000;
    INST_LDA_DIR(&c);
    printf("ACC A: %02x\n", c.a);

    printf("\nLDAA EXT\n");
    write_memory(&c, 0xC001, 0x30);
    write_memory(&c, 0xC002, 0x56);
    write_memory(&c, 0x3056, 0xFF);
    c.pc = 0xC000;
    INST_LDA_EXT(&c);
    printf("ACC A: %02x\n", c.a);

    c = (cpu) {0};

    printf("\nLoad Program\n");
    load_program(&c);
    write_memory(&c, 0x0, 0xFF);
    print_memory_range(&c, 0xC000, 10);

    printf("\nExec program\n");
    print_cpu_state(&c);
    exec_program(&c);

    line_to_instruction("ldaa 123\n");
}

