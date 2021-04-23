#ifndef EMULATOR_H
#define EMULATOR_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

#define MAX_MEMORY (1 << 16)
#define MAX_LABELS 0xFF
#define MAX_PORTS 5
#define FMT8 "0x%02x"
#define FMT16 "0x%04x"

typedef enum {
    NONE,
    IMMEDIATE,
    EXTENDED,
    DIRECT,
    INDEXDED_X,
    INDEXDED_Y,
    INHERENT,
    RELATIVE,
    OPERAND_TYPE_COUNT
} operand_type;

typedef struct {
    uint16_t value;
    operand_type type;
    uint8_t from_label;
} operand;

typedef enum {
    ORG,
    CONSTANT,
    RMB,
    FCC,
    LABEL,
    NOT_A_DIRECTIVE,
    DIRECTIVE_TYPE_COUNT
} directive_type;

typedef struct {
    const char *label;
    const char *opcode_str;
    operand operand;
    directive_type type;
} directive;

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
    union {
        struct {
            uint8_t s : 1;
            uint8_t x : 1;
            uint8_t h : 1;
            uint8_t i : 1;
            uint8_t n : 1;
            uint8_t z : 1;
            uint8_t v : 1;
            uint8_t c : 1;
        };
        uint8_t status;
    };

    uint8_t memory[MAX_MEMORY];

    uint8_t ports[MAX_PORTS];
    uint8_t ddrx[MAX_PORTS];

    directive labels[MAX_LABELS];
    uint8_t label_count;
} cpu;

#endif // EMUALTOR_H

#ifdef EMULATOR_IMPLEMENTATION

#define ERROR(f_, ...) do {\
    printf("[ERROR] l.%u: "f_".\n", file_line, __VA_ARGS__); \
    exit(1); \
} while (0)

#define INFO(f_, ...) printf("[INFO] "f_"\n", __VA_ARGS__)

static uint32_t file_line = 0;

typedef enum {
    CARRY = 0x1,
    OFLOW = 0x2,
    ZERO  = 0x4,
    NEG   = 0x8,
    IRQ   = 0x10,
    HALFC = 0x20,
    XIRQ  = 0x40,
    STOP  = 0x80,
} flags;

typedef enum {
    PORTA,
    PORTB,
    PORTC,
    PORTD,
    PORTE,
    PORTF,
    PORTG
} ports;

typedef enum {
    PORTA_ADDR = 0x1000,
    DDRA = 0x1001,
    PORTG_ADDR = 0x1002,
    DDRG = 0x1003,
    PORTB_ADDR = 0x1004,
    PORTF_ADDR = 0x1005,
    PORTC_ADDR = 0x1006,
    DDRC = 0x1007,
    PORTD_ADDR = 0x1008,
    DDRD = 0x1009,
    PORTE_ADDR = 0x100a,
} ports_addr;

typedef struct {
    uint8_t opcode;
    operand operand;
} mnemonic;

typedef struct {
    char *names[2]; // Some instructions have aliases like lda = ldaa
    uint8_t name_count;
    uint8_t codes[OPERAND_TYPE_COUNT];
    void (*func[OPERAND_TYPE_COUNT]) (cpu *cpu);
    operand_type operands[OPERAND_TYPE_COUNT];
    // The maximum value an operand in immediate addressing mode can have,
    // certain isntruction like 'LDA' can go up to 0xFF but others like 'LDS' can go up to 0xFFFF
    uint16_t immediate_16;
} instruction;


const char *directives_name[] = { "org", "equ" };
#define DIRECTIVE_COUNT ((uint8_t)(sizeof(directives_name) / sizeof(directives_name[0])))



void (*instr_func[0x100]) (cpu *cpu) = {0};

/*****************************
*        Instructions        *
*****************************/

uint8_t NEXT8(cpu *cpu) {
    return cpu->memory[++cpu->pc];
}

uint16_t NEXT16(cpu *cpu) {
    uint8_t b0 = NEXT8(cpu); // high order byte
    uint8_t b1 = NEXT8(cpu); // low order byte
    uint16_t joined = (b0 << 8) | b1;
    return joined;
}

uint8_t DIR_WORD(cpu *cpu) {
    // Get value of the next operand
    uint8_t addr = NEXT8(cpu);
    // Get value the operand is poiting at
    return cpu->memory[addr];
}

uint8_t EXT_WORD(cpu *cpu) {
    return cpu->memory[NEXT16(cpu)];
}

uint8_t STACK_POP8(cpu *cpu) {
    cpu->sp++;
    return cpu->memory[cpu->sp];
}

uint16_t STACK_POP16(cpu *cpu) {
    uint8_t v1 = STACK_POP8(cpu);
    uint8_t v2 = STACK_POP8(cpu);
    return (v1 << 8) | v2;
}

void STACK_PUSH8(cpu *cpu, uint8_t v) {
    cpu->memory[cpu->sp] = v & 0xFF;
    cpu->sp--;
}

void STACK_PUSH16(cpu *cpu, uint16_t v) {
    STACK_PUSH8(cpu, v & 0xFF);
    STACK_PUSH8(cpu, (v >> 8) & 0xFF);
}

void SET_FLAGS(cpu *cpu, int16_t result, uint8_t flags) {
    if (flags & CARRY) {
        cpu->c = (result > 0xFF) || (result < 0);
    }
    if (flags & OFLOW) {
        cpu->v = (result >= 127 || result <= -128);
    }
    if (flags & ZERO) {
        cpu->z = (result == 0);
    }
    if (flags & NEG) {
        cpu->n = (result >> 7) & 1;
    }
    // TODO: IRQ, HALF CARRY, XIRQ, STOP
}

void INST_NOP(cpu *cpu) {
    (void) cpu;
    // DOES NOTHING
}

void INST_CLV(cpu *cpu) {
    cpu->v = 0;
}
void INST_SEV(cpu *cpu) {
    cpu->v = 1;
}
void INST_CLC(cpu *cpu) {
    cpu->c = 0;
}
void INST_SEC(cpu *cpu) {
    cpu->c = 1;
}
void INST_CLI(cpu *cpu) {
    cpu->i = 0;
}
void INST_SEI(cpu *cpu) {
    cpu->i = 1;
}

uint16_t READ_FROM_PORTS(cpu *cpu, uint16_t addr) {
    if (addr == PORTA_ADDR) {
        uint8_t ret = cpu->ports[PORTA] & 0x7;
        // Or with ret only if third and seventh bit is 0 (input mode)
        ret |= cpu->ports[PORTA] & (~((cpu->memory[addr] >> 3) & 1) << 3);
        ret |= cpu->ports[PORTA] & (~((cpu->memory[addr] >> 7) & 1) << 7);
        return ret;
    }
    if (addr == PORTB_ADDR) { // Output only port
        return 0;
    }
    if (addr == PORTC_ADDR) {
        return cpu->ports[PORTC] & cpu->memory[DDRC];
    }
    if (addr == PORTD_ADDR) {
        return cpu->ports[PORTD] & cpu->memory[DDRD] & 0x70;
    }
    if (addr == PORTE_ADDR) { // Input only port
        return cpu->ports[PORTE];
    }

    return 0xFFFF;
}

void SET_LD_FLAGS(cpu *cpu, uint8_t result) {
    cpu->n = (result >> 7) & 1;
    cpu->z = result == 0;
    cpu->v = 0;
}

void INST_LDA_IMM(cpu *cpu) {
    // Operand is a constant, just load the next byte in memory
    uint8_t value = NEXT8(cpu);
    cpu->a = value;
    SET_LD_FLAGS(cpu, value);
}

void INST_LDA_DIR(cpu *cpu) {
    uint8_t value = DIR_WORD(cpu);
    cpu->a = value;
    SET_LD_FLAGS(cpu, value);
}

void INST_LDA_EXT(cpu *cpu) {
    uint16_t addr = NEXT16(cpu);
    uint16_t port_val = READ_FROM_PORTS(cpu, addr);
    if (port_val < 0xFF) { // < 0xFF means reading from a port
        cpu->a = port_val;
        SET_LD_FLAGS(cpu, port_val);
    } else {
        uint8_t value = cpu->memory[addr];
        cpu->a = value;
        SET_LD_FLAGS(cpu, value);
    }
}

void INST_LDB_IMM(cpu *cpu) {
    // Operand is a constant, just load the next byte in memory
    uint8_t value = NEXT8(cpu);
    cpu->b = value;
    SET_LD_FLAGS(cpu, value);
}

void INST_LDB_DIR(cpu *cpu) {
    uint8_t value = DIR_WORD(cpu);
    cpu->b = value;
    SET_LD_FLAGS(cpu, value);
}

void INST_LDB_EXT(cpu *cpu) {
    uint16_t addr = NEXT16(cpu);
    uint16_t port_val = READ_FROM_PORTS(cpu, addr);
    if (port_val < 0xFF) { // < 0xFF means reading from a port
        cpu->b = port_val;
        SET_LD_FLAGS(cpu, port_val);
    } else {
        uint8_t value = cpu->memory[addr];
        cpu->b = value;
        SET_LD_FLAGS(cpu, value);
    }
}

void INST_ABA(cpu *cpu) {
    int16_t result = cpu->a + cpu->b;
    cpu->a = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ADCA_IMM(cpu *cpu) {
    uint8_t v = NEXT8(cpu);
    int16_t result = cpu->a + v + cpu->c;
    cpu->a = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ADCA_DIR(cpu *cpu) {
    uint8_t v = DIR_WORD(cpu);
    int16_t result = cpu->a + v + cpu->c;
    cpu->a = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ADCA_EXT(cpu *cpu) {
    uint8_t v = EXT_WORD(cpu);
    int16_t result = cpu->a + v + cpu->c;
    cpu->a = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ADCB_IMM(cpu *cpu) {
    uint8_t v = NEXT8(cpu);
    int16_t result = cpu->b + v + cpu->c;
    cpu->b = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ADCB_DIR(cpu *cpu) {
    uint8_t v = DIR_WORD(cpu);
    int16_t result = cpu->b + v + cpu->c;
    cpu->b = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ADCB_EXT(cpu *cpu) {
    uint8_t v = EXT_WORD(cpu);
    int16_t result = cpu->b + v + cpu->c;
    cpu->b = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ADDA_IMM(cpu *cpu) {
    uint8_t v = NEXT8(cpu);
    int16_t result = cpu->a + v;
    cpu->a = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ADDA_DIR(cpu *cpu) {
    uint8_t v = DIR_WORD(cpu);
    int16_t result = cpu->a + v;
    cpu->a = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ADDA_EXT(cpu *cpu) {
    uint8_t v = EXT_WORD(cpu);
    int16_t result = cpu->a + v;
    cpu->a = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ADDB_IMM(cpu *cpu) {
    uint8_t v = NEXT8(cpu);
    int16_t result = cpu->b + v;
    cpu->b = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ADDB_DIR(cpu *cpu) {
    uint8_t v = DIR_WORD(cpu);
    int16_t result = cpu->b + v;
    cpu->b = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ADDB_EXT(cpu *cpu) {
    uint8_t v = EXT_WORD(cpu);
    int16_t result = cpu->b + v;
    cpu->b = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}


void INST_STA_DIR(cpu *cpu) {
    uint8_t addr = NEXT8(cpu);
    cpu->memory[addr] = cpu->a;
    SET_FLAGS(cpu, cpu->a, ZERO | NEG);
    cpu->v = 0;
}

uint8_t WRITE_TO_PORTS(cpu *cpu, uint16_t addr) {
    if (addr == PORTA_ADDR) { // PORT A
        cpu->ports[PORTA] = cpu->a & cpu->memory[DDRA]; // Only write where bits are in output mode
        return 1;
    }
    else if (addr == DDRA) { // DDRA
        cpu->memory[addr] = 0x70; // (0x70 == 0111 0000)
        cpu->memory[addr] |= ((cpu->a >> 3) & 1) << 3; // Copy third bit from ACC A to DDRA
        cpu->memory[addr] |= ((cpu->a >> 7) & 1) << 7; // Copy seventh bit from ACC A to DDRA
        return 1;
    }
    else if (addr == PORTG_ADDR) { // PORT G
        ERROR("%s", "PORT G NOT IMPLETEND");
        return 1;
    } else if (addr == DDRG) { // DDRG
        ERROR("%s", "DDRG NOT IMPLETEND");
        return 1;
    }
    else if (addr == PORTB_ADDR) { // PORT B Full output mode
        cpu->ports[PORTB] = cpu->a;
        return 1;
    }
    else if (addr == PORTF_ADDR) { // PORT F
        ERROR("%s", "PORT F NOT IMPLETEND");
        return 1;
    }
    else if (addr == PORTC_ADDR) { // PORT C
        cpu->ports[PORTC] = cpu->a & cpu->memory[DDRC]; // Only write where bits are in output mode
        return 1;
    }
    else if (addr == DDRC) { // DDRC
        cpu->memory[addr] = cpu->a;
        return 1;
    }
    else if (addr == PORTD_ADDR) { // PORT D (6 bits pin)
        cpu->ports[PORTD] = cpu->a & cpu->memory[DDRD]; // Only write where bits are in output mode
        return 1;
    }
    else if (addr == DDRD) { // DDRD
        cpu->memory[addr] = cpu->a & 0x3F; // Only keep the first 6 bits (3F = 0011 1111)
        return 1;
    }
    else if (addr == PORTE_ADDR) { // PORT E
    }
    return 0;
}

void INST_STA_EXT(cpu *cpu) {
    uint16_t addr = NEXT16(cpu);
    if (!WRITE_TO_PORTS(cpu, addr)) {
        cpu->memory[addr] = cpu->a;
    }
    SET_FLAGS(cpu, cpu->a, ZERO | NEG);
    cpu->v = 0;
}

void INST_STB_DIR(cpu *cpu) {
    uint8_t addr = NEXT8(cpu);
    cpu->memory[addr] = cpu->b;
    SET_FLAGS(cpu, cpu->a, ZERO | NEG);
    cpu->v = 0;
}

void INST_STB_EXT(cpu *cpu) {
    uint16_t addr = NEXT16(cpu);
    if (!WRITE_TO_PORTS(cpu, addr)) {
        cpu->memory[addr] = cpu->b;
    }
    SET_FLAGS(cpu, cpu->a, ZERO | NEG);
    cpu->v = 0;
}

void INST_BRA(cpu *cpu) {
    uint8_t jmp = NEXT8(cpu);
    cpu->pc += (int8_t) jmp;
}

void INST_BCC(cpu *cpu) {
    uint8_t jmp = NEXT8(cpu);
    if (cpu->c == 0) {
        cpu->pc += (int8_t) jmp;
    }
}

void INST_BCS(cpu *cpu) {
    uint8_t jmp = NEXT8(cpu);
    if (cpu->c == 1) {
        cpu->pc += (int8_t) jmp;
    }
}

void INST_BEQ(cpu *cpu) {
    uint8_t jmp = NEXT8(cpu);
    if (cpu->z == 0) {
        cpu->pc += (int8_t) jmp;
    }
}

void INST_BGE(cpu *cpu) {
    uint8_t jmp = NEXT8(cpu);
    if ((cpu->n ^ cpu->v) == 0) {
        cpu->pc += (int8_t) jmp;
    }
}

void INST_BGT(cpu *cpu) {
    uint8_t jmp = NEXT8(cpu);
    if (cpu->z + (cpu->n ^ cpu->v) == 0) {
        cpu->pc += (int8_t) jmp;
    }
}

void INST_BHI(cpu *cpu) {
    uint8_t jmp = NEXT8(cpu);
    if (cpu->c + cpu->z == 0) {
        cpu->pc += (int8_t) jmp;
    }
}

void INST_BLE(cpu *cpu) {
    uint8_t jmp = NEXT8(cpu);
    if (cpu->z + (cpu->n ^ cpu->c) != 0) {
        cpu->pc += (int8_t) jmp;
    }
}

void INST_BLS(cpu *cpu) {
    uint8_t jmp = NEXT8(cpu);
    if (cpu->c + cpu->z != 0) {
        cpu->pc += (int8_t) jmp;
    }
}

void INST_BLT(cpu *cpu) {
    uint8_t jmp = NEXT8(cpu);
    if (cpu->n + cpu->v != 0) {
        cpu->pc += (int8_t) jmp;
    }
}

void INST_BMI(cpu *cpu) {
    uint8_t jmp = NEXT8(cpu);
    if (cpu->n == 1) {
        cpu->pc += (int8_t) jmp;
    }
}

void INST_BNE(cpu *cpu) {
    uint8_t jmp = NEXT8(cpu);
    if (cpu->z == 0) {
        cpu->pc += (int8_t) jmp;
    }
}

void INST_BPL(cpu *cpu) {
    uint8_t jmp = NEXT8(cpu);
    if (cpu->n == 0) {
        cpu->pc += (int8_t) jmp;
    }
}

void INST_BRN(cpu *cpu) {
    uint8_t jmp = NEXT8(cpu);
    (void) jmp;
}

void INST_BVC(cpu *cpu) {
    uint8_t jmp = NEXT8(cpu);
    if (cpu->v == 0) {
        cpu->pc += (int8_t) jmp;
    }
}

void INST_BVS(cpu *cpu) {
    uint8_t jmp = NEXT8(cpu);
    if (cpu->v == 1) {
        cpu->pc += (int8_t) jmp;
    }
}

void INST_TAB(cpu *cpu) {
    cpu->b = cpu->a;
    SET_FLAGS(cpu, cpu->b, ZERO | NEG);
    cpu->v = 0;
}

void INST_TBA(cpu *cpu) {
    cpu->a = cpu->b;
    SET_FLAGS(cpu, cpu->b, ZERO | NEG);
    cpu->v = 0;
}

void SET_CMP_FLAGS(cpu *cpu, uint8_t a, uint8_t v) {
    int16_t r = a - v;
    SET_FLAGS(cpu, r, CARRY | OFLOW | ZERO | NEG);
}

void INST_CMPA_IMM(cpu *cpu) {
    uint8_t a = cpu->a;
    uint8_t v = NEXT8(cpu);
    SET_CMP_FLAGS(cpu, a, v);
}

void INST_CMPA_DIR(cpu *cpu) {
    uint8_t a = cpu->a;
    uint8_t v = DIR_WORD(cpu);
    SET_CMP_FLAGS(cpu, a, v);
}

void INST_CMPA_EXT(cpu *cpu) {
    uint8_t a = cpu->a;
    uint8_t v = EXT_WORD(cpu);
    SET_CMP_FLAGS(cpu, a, v);
}

void INST_CMPB_IMM(cpu *cpu) {
    uint8_t b = cpu->b;
    uint8_t v = NEXT8(cpu);
    SET_CMP_FLAGS(cpu, b, v);
}

void INST_CMPB_DIR(cpu *cpu) {
    uint8_t b = cpu->b;
    uint8_t v = DIR_WORD(cpu);
    SET_CMP_FLAGS(cpu, b, v);
}

void INST_CMPB_EXT(cpu *cpu) {
    uint8_t b = cpu->b;
    uint8_t v = EXT_WORD(cpu);
    SET_CMP_FLAGS(cpu, b, v);
}

void INST_LDS_IMM(cpu *cpu) {
    uint16_t v = NEXT16(cpu);
    SET_FLAGS(cpu, v, NEG | ZERO);
    cpu->v = 0;
    cpu->sp = v;
}

void INST_LDS_DIR(cpu *cpu) {
    uint16_t v = DIR_WORD(cpu);
    SET_FLAGS(cpu, v, NEG | ZERO);
    cpu->v = 0;
    cpu->sp = v;
}

void INST_LDS_EXT(cpu *cpu) {
    uint16_t v = EXT_WORD(cpu);
    SET_FLAGS(cpu, v, NEG | ZERO);
    cpu->v = 0;
    cpu->sp = v;
}

void INST_RTS_INH(cpu *cpu) {
    uint16_t ret_addr = STACK_POP16(cpu);
    cpu->pc = ret_addr;
}

void INST_JSR_DIR(cpu *cpu) {
    uint8_t sub_addr = DIR_WORD(cpu);
    STACK_PUSH16(cpu, cpu->pc);
    cpu->pc = sub_addr;
}

void INST_JSR_EXT(cpu *cpu) {
    uint16_t sub_addr = NEXT16(cpu);
    STACK_PUSH16(cpu, cpu->pc);
    cpu->pc = sub_addr;
}

void INST_PSHA_INH(cpu *cpu) {
    uint8_t a = cpu->a;
    STACK_PUSH8(cpu, a);
}

void INST_PSHB_INH(cpu *cpu) {
    uint8_t b = cpu->b;
    STACK_PUSH8(cpu, b);
}

void INST_PSHX_INH(cpu *cpu) {
    uint8_t x = cpu->x;
    STACK_PUSH16(cpu, x);
}

void INST_PULA_INH(cpu *cpu) {
    uint8_t v = STACK_POP8(cpu);
    cpu->a = v;
}

void INST_PULB_INH(cpu *cpu) {
    uint8_t v = STACK_POP8(cpu);
    cpu->b = v;
}

void INST_PULX_INH(cpu *cpu) {
    uint16_t v = STACK_POP16(cpu);
    cpu->x = v;
}

void INST_INCA_INH(cpu *cpu) {
    int16_t result = cpu->a + 1;
    cpu->a = result & 0xFF;
    SET_FLAGS(cpu, result, OFLOW | ZERO | NEG);
}

void INST_INCB_INH(cpu *cpu) {
    int16_t result = cpu->b + 1;
    cpu->b = result & 0xFF;
    SET_FLAGS(cpu, result, OFLOW | ZERO | NEG);
}

instruction instructions[] = {
    {
        .names = {"ldaa", "lda"}, .name_count = 2,
        .codes = {[IMMEDIATE]=0x86, [DIRECT]=0x96, [EXTENDED]=0xB6},
        .func =  {
            [IMMEDIATE]=INST_LDA_IMM,
            [DIRECT]=INST_LDA_DIR,
            [EXTENDED]=INST_LDA_EXT,
        },
        .operands = { IMMEDIATE, EXTENDED, DIRECT },
    },
    {
        .names = {"ldab", "ldb"}, .name_count = 2,
        .codes = {[IMMEDIATE]=0xC6, [DIRECT]=0xD6, [EXTENDED]=0xF6},
        .func =  {
            [IMMEDIATE]=INST_LDB_IMM,
            [DIRECT]=INST_LDB_DIR,
            [EXTENDED]=INST_LDB_EXT,
        },
        .operands = { IMMEDIATE, EXTENDED, DIRECT },
    },
    {
        .names = {"staa", "sta"}, .name_count = 2,
        .codes = {[DIRECT]=0x97, [EXTENDED]=0xB7},
        .func = {
            [DIRECT]=INST_STA_DIR,
            [EXTENDED]=INST_STA_EXT,
        },
        .operands = { DIRECT, EXTENDED }
    },
    {
        .names = {"stab", "stb"}, .name_count = 2,
        .codes = {[DIRECT]=0xD7, [EXTENDED]=0xF7},
        .func = {
            [DIRECT]=INST_STB_DIR,
            [EXTENDED]=INST_STB_EXT,
        },
        .operands = { DIRECT, EXTENDED }
    },
    {
        .names = {"aba"}, .name_count = 1,
        .codes = {[INHERENT]=0x1B},
        .func = {[INHERENT]=INST_ABA},
        .operands = { INHERENT }
    },
    {
        .names = {"adca"}, .name_count = 1,
        .codes = {[IMMEDIATE]=0x89, [DIRECT]=0x99, [EXTENDED]=0xB9},
        .func =  {
            [IMMEDIATE]=INST_ADCA_IMM,
            [DIRECT]=INST_ADCA_DIR,
            [EXTENDED]=INST_ADCA_EXT
        },
        .operands = { IMMEDIATE, EXTENDED, DIRECT },
    },
    {
        .names = {"adcb"}, .name_count = 1,
        .codes = {[IMMEDIATE]=0xC9, [DIRECT]=0xD9, [EXTENDED]=0xF9},
        .func =  {
            [IMMEDIATE]=INST_ADCB_IMM,
            [DIRECT]=INST_ADCB_DIR,
            [EXTENDED]=INST_ADCB_EXT
        },
        .operands = { IMMEDIATE, EXTENDED, DIRECT },
    },
    {
        .names = {"adda"}, .name_count = 1,
        .codes = {[IMMEDIATE]=0x8B, [DIRECT]=0x9B, [EXTENDED]=0xBB},
        .func =  {
            [IMMEDIATE]=INST_ADDA_IMM,
            [DIRECT]=INST_ADDA_DIR,
            [EXTENDED]=INST_ADDA_EXT
        },
        .operands = { IMMEDIATE, EXTENDED, DIRECT },
    },
    {
        .names = {"addb"}, .name_count = 1,
        .codes = {[IMMEDIATE]=0xCB, [DIRECT]=0xDB, [EXTENDED]=0xFB},
        .func =  {
            [IMMEDIATE]=INST_ADDB_IMM,
            [DIRECT]=INST_ADDB_DIR,
            [EXTENDED]=INST_ADDB_EXT
        },
        .operands = { IMMEDIATE, DIRECT },
    },
    {
        .names = {"tab"}, .name_count = 1,
        .codes = {[INHERENT]=0x16},
        .func = {[INHERENT]=INST_TAB},
        .operands = { INHERENT }
    },
    {
        .names = {"tba"}, .name_count = 1,
        .codes = {[INHERENT]=0x17},
        .func = {[INHERENT]=INST_TBA},
        .operands = { INHERENT }
    },
    {
        .names = {"cmpa"}, .name_count = 1,
        .codes = {[IMMEDIATE]=0x81, [DIRECT]=0x91, [EXTENDED]=0xB1},
        .func =  {
            [IMMEDIATE]=INST_CMPA_IMM,
            [DIRECT]=INST_CMPA_DIR,
            [EXTENDED]=INST_CMPA_EXT,
        },
        .operands = { IMMEDIATE, DIRECT, EXTENDED },
        .immediate_16 = 1
    },
    {
        .names = {"cmpb"}, .name_count = 1,
        .codes = {[IMMEDIATE]=0xC1, [DIRECT]=0xD1, [EXTENDED]=0xE1},
        .func =  {
            [IMMEDIATE]=INST_CMPB_IMM,
            [DIRECT]=INST_CMPB_DIR,
            [EXTENDED]=INST_CMPB_EXT,
        },
        .operands = { IMMEDIATE, DIRECT, EXTENDED },
        .immediate_16 = 1
    },
    {
        .names = {"bcc", "bhs"}, .name_count = 2,
        .codes = {[RELATIVE]=0x24},
        .func = {[RELATIVE]=INST_BCC},
        .operands = { RELATIVE }
    },
    {
        .names = {"bcs", "blo"}, .name_count = 2,
        .codes = {[RELATIVE]=0x25},
        .func = {[RELATIVE]=INST_BCS},
        .operands = { RELATIVE }
    },
    {
        .names = {"beq"}, .name_count = 1,
        .codes = {[RELATIVE]=0x27},
        .func = {[RELATIVE]=INST_BEQ},
        .operands = { RELATIVE }
    },
    {
        .names = {"bge"}, .name_count = 1,
        .codes = {[RELATIVE]=0x2C},
        .func = {[RELATIVE]=INST_BGE},
        .operands = { RELATIVE }
    },
    {
        .names = {"bgt"}, .name_count = 1,
        .codes = {[RELATIVE]=0x2E},
        .func = {[RELATIVE]=INST_BGT},
        .operands = { RELATIVE }
    },
    {
        .names = {"bhi"}, .name_count = 1,
        .codes = {[RELATIVE]=0x22},
        .func = {[RELATIVE]=INST_BHI},
        .operands = { RELATIVE }
    },
    {
        .names = {"ble"}, .name_count = 1,
        .codes = {[RELATIVE]=0x2F},
        .func = {[RELATIVE]=INST_BLE},
        .operands = { RELATIVE }
    },
    {
        .names = {"bls"}, .name_count = 1,
        .codes = {[RELATIVE]=0x23},
        .func = {[RELATIVE]=INST_BLS},
        .operands = { RELATIVE }
    },
    {
        .names = {"blt"}, .name_count = 1,
        .codes = {[RELATIVE]=0x2D},
        .func = {[RELATIVE]=INST_BLT},
        .operands = { RELATIVE }
    },
    {
        .names = {"bmi"}, .name_count = 1,
        .codes = {[RELATIVE]=0x2B},
        .func = {[RELATIVE]=INST_BMI},
        .operands = { RELATIVE }
    },
    {
        .names = {"bne"}, .name_count = 1,
        .codes = {[RELATIVE]=0x26},
        .func = {[RELATIVE]=INST_BNE},
        .operands = { RELATIVE }
    },
    {
        .names = {"bpl"}, .name_count = 1,
        .codes = {[RELATIVE]=0x2A},
        .func = {[RELATIVE]=INST_BPL},
        .operands = { RELATIVE }
    },
    {
        .names = {"bra"}, .name_count = 1,
        .codes = {[RELATIVE]=0x20},
        .func = {[RELATIVE]=INST_BRA},
        .operands = { RELATIVE }
    },
    {
        .names = {"brn"}, .name_count = 1,
        .codes = {[RELATIVE]=0x21},
        .func = {[RELATIVE]=INST_BRN},
        .operands = { RELATIVE }
    },
    {
        .names = {"bvc"}, .name_count = 1,
        .codes = {[RELATIVE]=0x28},
        .func = {[RELATIVE]=INST_BVC},
        .operands = { RELATIVE }
    },
    {
        .names = {"bvs"}, .name_count = 1,
        .codes = {[RELATIVE]=0x29},
        .func = {[RELATIVE]=INST_BVS},
        .operands = { RELATIVE }
    },
    {
        .names = {"clv"}, .name_count = 1,
        .codes = {[NONE]=0x0A},
        .func = {[NONE]=INST_CLV},
        .operands = { NONE }
    },
    {
        .names = {"sev"}, .name_count = 1,
        .codes = {[NONE]=0x0B},
        .func = {[NONE]=INST_SEV},
        .operands = { NONE }
    },
    {
        .names = {"clc"}, .name_count = 1,
        .codes = {[NONE]=0x0C},
        .func = {[NONE]=INST_CLC},
        .operands = { NONE }
    },
    {
        .names = {"sec"}, .name_count = 1,
        .codes = {[NONE]=0x0D},
        .func = {[NONE]=INST_SEC},
        .operands = { NONE }
    },
    {
        .names = {"cli"}, .name_count = 1,
        .codes = {[NONE]=0x0E},
        .func = {[NONE]=INST_CLI},
        .operands = { NONE }
    },
    {
        .names = {"sei"}, .name_count = 1,
        .codes = {[NONE]=0x0F},
        .func = {[NONE]=INST_SEI},
        .operands = { NONE }
    },
    {
        .names = {"lds"}, .name_count = 1,
        .codes = {[IMMEDIATE]=0x8E, [DIRECT]=0x9E,[EXTENDED]=0xBE},
        .func = {
            [IMMEDIATE]=INST_LDS_IMM,
            [DIRECT]=INST_LDS_DIR,
            [EXTENDED]=INST_LDS_EXT
        },
        .operands = { IMMEDIATE, DIRECT, EXTENDED },
        .immediate_16 = 1
    },
    {
        .names = {"rts"}, .name_count = 1,
        .codes = {[INHERENT]=0x39},
        .func = { [INHERENT]=INST_RTS_INH},
        .operands = { INHERENT },
    },
    {
        .names = {"jsr"}, .name_count = 1,
        .codes = {[DIRECT]=0x9D, [EXTENDED]=0xBD},
        .func = {
            [DIRECT]=INST_JSR_DIR,
            [EXTENDED]=INST_JSR_EXT,
        },
        .operands = { DIRECT, EXTENDED},
    },
    {
        .names = {"psha"}, .name_count = 1,
        .codes = {[INHERENT]=0x36},
        .func = { [INHERENT]=INST_PSHA_INH},
        .operands = { INHERENT },
    },
    {
        .names = {"pshb"}, .name_count = 1,
        .codes = {[INHERENT]=0x37},
        .func = { [INHERENT]=INST_PSHB_INH},
        .operands = { INHERENT },
    },
    {
        .names = {"pshx"}, .name_count = 1,
        .codes = {[INHERENT]=0x3C},
        .func = { [INHERENT]=INST_PSHX_INH},
        .operands = { INHERENT },
    },
    {
        .names = {"pula"}, .name_count = 1,
        .codes = {[INHERENT]=0x32},
        .func = { [INHERENT]=INST_PULA_INH},
        .operands = { INHERENT },
    },
    {
        .names = {"pulb"}, .name_count = 1,
        .codes = {[INHERENT]=0x33},
        .func = { [INHERENT]=INST_PULB_INH},
        .operands = { INHERENT },
    },
    {
        .names = {"pulx"}, .name_count = 1,
        .codes = {[INHERENT]=0x38},
        .func = { [INHERENT]=INST_PULX_INH},
        .operands = { INHERENT },
    },
    {
        .names = {"inca"}, .name_count = 1,
        .codes = {[INHERENT]=0x4C},
        .func = { [INHERENT]=INST_INCA_INH},
        .operands = { INHERENT },
    },
    {
        .names = {"incb"}, .name_count = 1,
        .codes = {[INHERENT]=0x5C},
        .func = { [INHERENT]=INST_INCB_INH},
        .operands = { INHERENT },
    },
};

#define INSTRUCTION_COUNT ((uint8_t)(sizeof(instructions) / sizeof(instructions[0])))


/*****************************
*           Utils            *
*****************************/

void free_cpu(cpu *cpu) {
    for (uint8_t i = 0; i < cpu->label_count; ++i) {
        free((void *)cpu->labels[i].label);
    }
}

void add_instructions_func() {
    memset(instr_func, 0, 0x100 * sizeof(void*));
    for (uint8_t i = 0; i < INSTRUCTION_COUNT; ++i) {
        instruction *inst = &instructions[i];
        operand_type *type = inst->operands;
        while (*type != NONE) {
            instr_func[inst->codes[*type]] = inst->func[*type];
            type++;
        }
    }
}

uint8_t str_prefix(const char *str, const char *pre)
{
    return strncmp(pre, str, strlen(pre)) == 0;
}

uint8_t is_str_in_parts(const char *str, char **parts, uint8_t parts_count) {
    for (uint8_t i = 0; i < parts_count; i++) {
        if (parts[i] == NULL) continue;
        if (strcmp(parts[i], str) == 0) return 1;
    }
    return 0;
}

const char *strdup(const char *base) {
    size_t len = strlen(base);
    char *str = malloc(len + 1 * sizeof(char));
    if (str == NULL) {
        ERROR("%s", "calloc");
    }
    str[len] = '\0';
    return memcpy(str, base, len);
}

uint8_t is_valid_operand_type(instruction *inst, operand_type type) {
    operand_type *base = inst->operands;
    while(*base != NONE) {
        if (*base == type) {
            return 1;
        }
        base++;
    }
    return 0;
}

int str_empty(const char *str) {
    do {
        if (*str != '\0' && *str != ' ' && *str != '\n') return 0;
    } while(*str++ != '\0');
    return 1;
}

directive *get_directive_by_label(const char *label, directive *labels, uint8_t label_count) {
    for (uint8_t i = 0; i < label_count; ++i) {
        if (strcmp(label, labels[i].label) == 0) {
            return &labels[i];
        }
    }
    return NULL;
}

uint8_t is_directive(const char *str) {
    for (uint8_t i = 0; i < DIRECTIVE_COUNT; ++i) {
        const char *substr = strstr(str, directives_name[i]);
        if (substr != NULL) {
            // Verify if the word is surrouned by spaces (and not inside a word)
            if ((substr >= str || *(substr - 1) == ' ') && *(substr + strlen(directives_name[i])) == ' ') {
                return 1;
            }
        }
    }
    return 0;
}

void str_tolower(char *str) {
    for (uint8_t i = 0; *str != '\0'; ++i, ++str) {
        *str = tolower(*str);
    }
}

uint8_t split_by_space(char *str, char **out, uint8_t n) {
    assert(str);
    assert(n);
    uint8_t nb_parts = 0;
    uint8_t state = 0;
    while (*str) {
        if (*str == ';' || *str == '*' ||(*str == '/' && *(str + 1) == '/')) { // If start of a comment
            return nb_parts;
        }
        if (*str == ' ' || *str == '\n' || *str == '\t' || *str == '\r') {
            if (state == 1) {
                *str = '\0';
            } else {
                // Means there is no label on this line
                if (nb_parts == 0) {
                    out[0] = NULL;
                    nb_parts++;
                }
            }
            state = 0;
        } else if (state == 0) {
            state = 1;
            if (out != NULL) {
                out[nb_parts] = str;
            }
            nb_parts++;
            if (nb_parts > n) {
                printf("WARNING: Too much words on the same line (over %d)!\n", n);
                return nb_parts;
            }
        }
        str++;
    }
    return nb_parts;
}

void set_default_ddr(cpu *cpu) {
    cpu->memory[DDRA] = 0xF8;
    cpu->memory[DDRC] = 0xFF;
    cpu->memory[DDRD] = 0xFF;
}

const char *operand_type_as_str(operand_type type) {
    switch (type) {
        case NONE:       return "NONE";
        case IMMEDIATE:  return "IMMEDIATE";
        case EXTENDED:   return "EXTENDED";
        case DIRECT:     return "DIRECT";
        case INDEXDED_X: return "INDEXDED_X";
        case INDEXDED_Y: return "INDEXDED_Y";
        case INHERENT:   return "INHERENT";
        case RELATIVE:   return "RELATIVE";
        default: ERROR("UNKNOW OPERAND TYPE %d", type);
    }
}

/*****************************
*          Assembly          *
*****************************/

// Convert the given str to its opcode
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

// Returns operand type based on prefix and operand_value
operand_type get_operand_type(const char *str) {
    if (str[0] == '#')                  return IMMEDIATE;
    if (str[0] == '<' && str[1] == '$') return DIRECT;
    if (str[0] == '>' && str[1] == '$') return EXTENDED;
    if (str[0] == '$')                  return EXTENDED;
    return NONE;
}

uint16_t convert_str_from_base(const char *str, uint8_t base) {
    char *end;
    long l = strtol(str, &end, base);
    if ((l == 0 && str == end) || *end != '\0') {
        ERROR("%s is not a valid number for this base (%d)", str, base);
    }
    if (l > 0xFFFF) {
        ERROR("%s %s", str, "is > 0xFFFF");
    }
    return l;
}

uint16_t hex_str_to_u16(const char *str) {
    str += 2; // skips the #$
    return convert_str_from_base(str, 16);
}

uint16_t dec_str_to_u16(const char *str) {
    str += 1; // skips the #
    return convert_str_from_base(str, 10);
}

uint16_t bin_str_to_u16(const char *str) {
    str += 2; // skips the #$
    return convert_str_from_base(str, 2);
}

operand get_operand_value(const char *str, directive *labels, uint8_t label_count) {
    directive *directive = NULL;
    if (labels) {
        directive = get_directive_by_label(str, labels, label_count);
    }

    if (directive != NULL) {
        return (operand) {directive->operand.value, directive->operand.type, 1};
    }

    uint8_t offset = str[0] == '<' || str[0] == '>';
    uint16_t operand_value = 0;
    if (offset == 0 && str[0] == '#') {
        if (str[1] == '$') { // Hexadecimal
            operand_value = hex_str_to_u16(str);
        } else if (str[1] == '%') { // Binary
            operand_value = bin_str_to_u16(str);
        } else if (str[1] >= '0' && str[1] <= '9') { // Decimal
            operand_value = dec_str_to_u16(str);
        } else { // In case there is a wrong prefix, like #:, which does not exists
            ERROR("%s %s", str, "is not a valid operand");
        }
    } else if (str[offset] == '$') {
        operand_value = convert_str_from_base(str + offset + 1, 16);
    } else {
        ERROR("Invalid prefix for operand %s", str);
    }

    // Convert to a number
    operand_type type = get_operand_type(str);
    if (type == DIRECT && operand_value > 0xFF) {
        ERROR("Direct addressing mode only allows value up to 0xFF, recieved "FMT16, operand_value);
    }
    if (type == NONE) {
        ERROR("The operand `%s` is neither a constant or a label", str);
    }
    return (operand) {operand_value, type, 0};
}

mnemonic line_to_mnemonic(char *line, directive *labels, uint8_t label_count, uint16_t addr) {
    char *parts[5] = {0};
    uint8_t nb_parts = split_by_space(line, parts, 5);
    if (nb_parts == 0) {
        return (mnemonic) {0, {0, 0, 0}};
    }

    // If there is only a label
    if (nb_parts == 1 && parts[0] != NULL) {
        return (mnemonic) {0, {0, 0, 0}};
    }

    nb_parts--; // Does as if there was no label

    instruction *inst = opcode_str_to_hex(parts[1]);
    if (inst == NULL) {
        ERROR("%s is an undefined (or not implemented) instruction", parts[1]);
    }

    uint8_t need_operand = inst->operands[0] != NONE && inst->operands[0] != INHERENT;
    if (need_operand != (nb_parts - 1)) { // need an operand but none were given
        ERROR("%s instruction requires %d operand but %d recieved\n", parts[1], need_operand, nb_parts - 1);
    }

    mnemonic result = {0};
    if (need_operand) {
        result.operand = get_operand_value(parts[2], labels, label_count);
        if (inst->operands[0] == RELATIVE) {
            uint16_t operand_value = result.operand.value;
            if (result.operand.from_label) {
                int8_t offset = result.operand.value - addr - 1;
                operand_value = offset;
            } else {
                if (result.operand.value > 0xFF) {
                    ERROR("Relative addressing mode only supports 8 bits operands ("FMT16">0xFF)",
                          result.operand.value);
                }
            }
            result.operand.type = RELATIVE;
            result.operand.value = operand_value & 0xFF;
        }
        else if (result.operand.type == IMMEDIATE && inst->immediate_16 == 0 && result.operand.value > 0xFF) {
            ERROR("%s instruction can only go up to 0xFF, given value is "FMT16, parts[1], result.operand.value);
        }
        // Checks if the given addressig mode is used by this instruction
        else if (!is_valid_operand_type(inst, result.operand.type)) {
            ERROR("%s does not support %s addressing mode\n", parts[1], operand_type_as_str(result.operand.type));
        }
    } else if (inst->operands[0] == INHERENT) {
        result.operand.type = INHERENT;
    }

    result.opcode = inst->codes[result.operand.type];
    return result;
}

uint8_t add_mnemonic_to_memory(cpu *cpu, mnemonic *m, uint16_t addr) {
    uint8_t written = 0; // Number of bytes written
    cpu->memory[addr + (written++)] = m->opcode;
    if (m->operand.type != NONE && m->operand.type != INHERENT) {
        // TODO: Certain instruction such as CPX uses 2 operands even for immediate mode
        if (m->operand.value > 0xFF) {
            cpu->memory[addr + written++] = (m->operand.value >> 8) & 0xFF;
        }
        cpu->memory[addr + written++] = m->operand.value & 0xFF;
    }
    return written;
}

directive line_to_directive(char *line, directive *labels, uint8_t label_count) {
    char *parts[5] = {0};
    uint8_t nb_parts = split_by_space(line, parts, 5);
    if (nb_parts == 0) {
        return (directive) {NULL, NULL, {0, NONE, 0}, NOT_A_DIRECTIVE};
    }

    // If there is a label
    if (is_str_in_parts("equ", parts, nb_parts)) {
        if (nb_parts != 3) {
            ERROR("%s", "equ format : <LABEL> equ <VALUE>");
        }
        operand operand = get_operand_value(parts[2], labels, label_count);
        return (directive) {strdup(parts[0]), NULL, {operand.value, operand.type, operand.from_label}, CONSTANT};
    }
    if (is_str_in_parts("org", parts, nb_parts)) {
        if (nb_parts != 3) {
            ERROR("%s", "ORG format : [LABEL] ORG <ADDR> ($<VALUE>)");
        }
        operand operand = get_operand_value(parts[2], labels, label_count);
        if (operand.type == NONE) {
            ERROR("%s", "No operand found\n");
        }
        return (directive) {NULL, NULL, {operand.value, EXTENDED, operand.from_label}, ORG};
    }

    if (parts[0] != NULL) {
        return (directive) {strdup(parts[0]), parts[1], {0, EXTENDED, 1}, LABEL};
    }

    return (directive) {NULL, parts[1], {0, NONE, 0}, NOT_A_DIRECTIVE};
}

void load_program(cpu *cpu, const char *file_path) {
    FILE *f = fopen(file_path, "r");
    if (!f) {
        ERROR("Error while opennig file : %s\n", file_path);
    }

    // First PASS
    // On the first we get all the labels and directives which enables the use of labels that are later in the code.

    char buf[100];
    uint16_t addr = 0x0;
    for (;;) {
        file_line++;
        if (fgets(buf, 100, f) == NULL) {
            break;
        }

        // Remove \n from buffer
        buf[strcspn(buf, "\n")] = '\0';
        if (str_empty(buf)) {
            continue;
        }

        str_tolower(buf);
        directive d = line_to_directive(buf, cpu->labels, cpu->label_count);
        if (d.type == CONSTANT) {
            cpu->labels[cpu->label_count++] = d;
        } else if (d.type == ORG) {
            addr = d.operand.value;
        } else if (d.type == LABEL) {
            d.operand.value = addr;
            cpu->labels[cpu->label_count++] = d;
        }

        if (d.opcode_str != NULL) {
            instruction *inst = opcode_str_to_hex(d.opcode_str);
            if (inst == NULL) { continue; } // Unknow instruction

            addr++;
            for (uint8_t i = 0; i < OPERAND_TYPE_COUNT; ++i) {
                if (inst->operands[i] == DIRECT || (inst->operands[i] == IMMEDIATE && !inst->immediate_16)) {
                    addr += 1;
                    break;
                }
                if (inst->operands[i] == EXTENDED || (inst->operands[i] == IMMEDIATE && inst->immediate_16)) {
                    addr += 2;
                    break;
                }
            }
        }
    }

    // Reset file reading progress
    rewind(f);
    addr = 0x0;
    file_line = 0;

    cpu->pc = addr;
    // Second PASS
    // On the second pass, we convert all instructions to it's opcode.
    for (;;) {
        file_line++;
        if (fgets(buf, 100, f) == NULL) {
            break;
        }

        // Remove \n from buffer
        buf[strcspn(buf, "\n")] = '\0';
        if (str_empty(buf)) {
            continue;
        }
        str_tolower(buf);
        if (is_directive(buf)) {
            directive d = line_to_directive(buf, cpu->labels, cpu->label_count);
            if (d.type == ORG) {
                addr = d.operand.value;
                if (cpu->pc == 0x0) {
                    cpu->pc = addr;
                }
            }
            free((void*) d.label);
            continue;
        }
        mnemonic m = line_to_mnemonic(buf, cpu->labels, cpu->label_count, addr);
        if (m.opcode == 0) {
            continue;
        }
        addr += add_mnemonic_to_memory(cpu, &m, addr);
    }
    fclose(f);
}

void exec_program(cpu *cpu) {
    while (cpu->memory[cpu->pc] != 0x00) {
        uint8_t inst = cpu->memory[cpu->pc];
        if (instr_func[inst] != NULL) {
            (*instr_func[inst])(cpu); // Call the function with this opcode
        }
        cpu->pc++;
    }
}
#endif // EMULATOR_IMPLEMENTATION
