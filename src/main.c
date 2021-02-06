#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define MAX_MEMORY (1 << 16)
#define FMT8 "0x%02x"
#define FMT16 "0x%04x"

typedef enum {
    IMMEDIATE,
    EXTENDED,
    DIRECT,
    INDEXDED_X,
    INDEXDED_Y,
    INHERENT,
    RELATIVE,
    NONE,
    OPERAND_TYPE_COUNT
} operand_type;

typedef struct {
    char *names[2]; // Some instructions have aliases like lda = ldaa
    uint8_t name_count;
    uint8_t codes[OPERAND_TYPE_COUNT];
    operand_type operands[OPERAND_TYPE_COUNT];
} instruction;

typedef struct {
    uint8_t opcode;
    uint16_t operand;
    operand_type operand_type;
} mnemonic;

mnemonic nop_mnemonic = {.opcode = 0x1, .operand = 0, .operand_type = NONE};

instruction instructions[] = {
    {
        .names = {"ldaa", "lda"}, .name_count = 2,
        .codes = {[IMMEDIATE]=0x86, [DIRECT]=0x96, [EXTENDED]=0xB6},
        .operands = { IMMEDIATE, EXTENDED, DIRECT }
    },
    {
        .names = {"ldab", "ldb"}, .name_count = 2,
        .codes = {[IMMEDIATE]=0xC6, [DIRECT]=0xD6, [EXTENDED]=0xF6},
        .operands = { IMMEDIATE, EXTENDED, DIRECT }
    },
    {
        .names = {"aba"}, .name_count = 1,
        .codes = {[INHERENT]=0x1B},
        .operands = { INHERENT }
    },
};
#define INSTRUCTION_COUNT ((uint8_t)(sizeof(instructions) / sizeof(instructions[0])))


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

/*****************************
*        Instructions        *
*****************************/

void INST_NOP(cpu *cpu) {
    (void) cpu;
    // DOES NOTHING
}

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

void INST_LDB_IMM(cpu *cpu) {
    uint8_t value = cpu->memory[++cpu->pc]; // Operand is a constant, just load the next byte in memory
    printf("Loading " FMT8 " in BCC B\n", value);
    cpu->b = value;
}

void INST_LDB_DIR(cpu *cpu) {
    uint8_t addr = cpu->memory[++cpu->pc]; // Get value of the next operand
    printf("Loading value at address " FMT8 " (= " FMT8 ") in BCC B\n", addr, cpu->memory[addr]);
    cpu->b = cpu->memory[addr]; // Get value the operand is poiting at
}

void INST_LDB_EXT(cpu *cpu) {
    uint8_t b0 = cpu->memory[++cpu->pc]; // high order byte
    uint8_t b1 = cpu->memory[++cpu->pc]; // low order byte
    uint16_t addr = (b0 << 8) | b1;
    printf("Loading value at address " FMT16 " (= " FMT8 ") in BCC B\n", addr, cpu->memory[addr]);
    cpu->b = cpu->memory[addr];
}

void INST_ABA(cpu *cpu) {
    uint8_t result = (cpu->a + cpu->b) & 0xFF;
    cpu->a = result;
}


/*****************************
*           Utils            *
*****************************/

void print_memory_range(cpu *cpu, uint16_t from, uint16_t len) {
    for (uint16_t i = 0; i < len; ++i) {
        printf("%04x: %02x\n", i, cpu->memory[from + i]);
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

instruction * opcode_str_to_hex(const char *str) {
    for (int i = 0; i < INSTRUCTION_COUNT; ++i) {
        instruction *op = &instructions[i];
        for (int j = 0; j < op->name_count; ++j) {
            if (strcmp(str, op->names[j]) == 0) {
                return op;
            }
        }
    }
    return NULL;
}

mnemonic line_to_mnemonic(const char *line) {
    uint8_t nb_parts = part_count(line);
    if (nb_parts > 2) {
        printf("Too many operands\n");
        return nop_mnemonic;
    }

    // Get instruction name
    char part[6] = {0}; // Longest instruction is BRCLR
    int i; // index of part to write
    for (i = 0; *line != ' ' && *line != '\0' && i < 6; ++i, ++line) {
        part[i] = tolower(*line);
    }
    if (i > 5) {
        printf("Instruction is too long\n");
        return nop_mnemonic;
    }

    instruction *inst = opcode_str_to_hex(part);
    if (inst == NULL) {
        printf("%s is an undefined (or not implemented) instruction\n", part);
        return nop_mnemonic;
    }

    uint8_t need_operand = inst->operands[0] != NONE && inst->operands[0] != INHERENT;
    if (need_operand != (nb_parts - 1)) { // need an operand but none were given
        printf("%s instruction requires %d operand but %d recieved\n", part, need_operand, nb_parts - 1);
        return nop_mnemonic;
    }
    mnemonic result = {0};
    if (need_operand) {
        char operand_str[6] = {0};
        while (*line == ' ') line++; // skip all spaces until the next character
        // Copy operand to operand_str
        for (int i = 0; i < 6 && *line != '\0' && *line != ' '; ++i, ++line) operand_str[i] = *line;

        // Find where the number starts (skip # and $)
        const char *number_part = operand_str;
        while (!isdigit(*number_part)) number_part++;
        // Convert to a number
        char *end;
        long l = strtol(number_part, &end, 16);
        if (l == 0 && number_part == end) {
            printf("Error while parsing the operand %s\n", operand_str);
            return nop_mnemonic;
        }
        uint16_t operand_value = l & 0xFFFF; // Keep the value below 0xFFFF
        result.operand = operand_value;
        // Change operand type based on prefix and operand_value
        if (operand_str[0] == '#') result.operand_type = IMMEDIATE;
        if (operand_str[0] == '$' && operand_value <= 0xFF) result.operand_type = DIRECT;
        if (operand_str[0] == '$' && operand_value >  0xFF) result.operand_type = EXTENDED;
    }
    result.opcode = inst->codes[inst->operands[result.operand_type]];

    return result;
}

void add_mnemonic_to_memory(cpu *cpu, mnemonic *m) {
    cpu->memory[cpu->pc++] = m->opcode;
    if (m->operand_type != NONE) {
        // Only extended uses 2 bytes for the operand
        // TODO: Certain instruction such as CPX uses 2 operands even for immediate mode
        if (m->operand_type == EXTENDED) {
            cpu->memory[cpu->pc++] = (m->operand >> 8) & 0xFF;
            cpu->memory[cpu->pc++] = m->operand & 0xFF;
        } else {
            cpu->memory[cpu->pc++] = m->operand & 0xFF;
        }
    }
}

void load_program(cpu *cpu) {
    const int org = 0xC000; // TODO: SHOULD BE DETERMINED IN THE CODE
    cpu->pc = org;
    mnemonic m1 = line_to_mnemonic("ldaa #20\n");
    add_mnemonic_to_memory(cpu, &m1);

    mnemonic m2 = line_to_mnemonic("ldb #30\n");
    add_mnemonic_to_memory(cpu, &m2);

    mnemonic m3 = line_to_mnemonic("aba");
    add_mnemonic_to_memory(cpu, &m3);
    cpu->pc = org;
}

void (*instr_func[0xFF]) (cpu *cpu) = {INST_NOP};

void exec_program(cpu *cpu) {
    while (cpu->memory[cpu->pc] != 0x0) {
        uint8_t inst = cpu->memory[cpu->pc];
        printf("Executing "FMT8" instruction\n", inst);
        if (instr_func[inst] != NULL) {
            (*instr_func[inst])(cpu); // Call the function with this opcode
        }
        cpu->pc++;
    }
}


int main() {
    instr_func[0x86] = INST_LDA_IMM;
    instr_func[0x96] = INST_LDA_DIR;
    instr_func[0xB6] = INST_LDA_EXT;
    instr_func[0xC6] = INST_LDB_IMM;
    instr_func[0xD6] = INST_LDB_DIR;
    instr_func[0xF6] = INST_LDB_EXT;
    instr_func[0x1B] = INST_ABA;
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

    printf("\nstring to mnemonic\n");
    mnemonic m = line_to_mnemonic("ldaa #$20FF\n");
    printf(FMT8" "FMT16"\n", m.opcode, m.operand);

    // Clear the cpu
    c = (cpu) {0};

    printf("\nLoad Program\n");
    load_program(&c);
    print_memory_range(&c, 0xC000, 10);

    printf("\nExec program\n");
    write_memory(&c, 0x20, 0xFF);
    write_memory(&c, 0x3000, 0x30);
    printf("\nStarting execution\n");
    exec_program(&c);
}

