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

#define u8 uint8_t
#define u16 uint16_t
#define i8 int8_t
#define i16 int16_t
#define u32 uint32_t

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
    u16 value;
    operand_type type;
    u8 from_label;
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
            u8 b; // B is low order
            u8 a; // A is high order
        };
        u16 d;
    };
    u16 sp;
    u16 pc;
    union {
        struct {
            u8 s : 1;
            u8 x : 1;
            u8 h : 1;
            u8 i : 1;
            u8 n : 1;
            u8 z : 1;
            u8 v : 1;
            u8 c : 1;
        };
        u8 status;
    };

    u8 memory[MAX_MEMORY];

    u8 ports[MAX_PORTS];
    u8 ddrx[MAX_PORTS];

    directive labels[MAX_LABELS];
    u8 label_count;
} cpu;

#endif // EMUALTOR_H

#ifdef EMULATOR_IMPLEMENTATION

#define ERROR(f_, ...) do {\
    printf("[ERROR] l.%u: "f_".\n", file_line, __VA_ARGS__); \
    exit(1); \
} while (0)

#define INFO(f_, ...) printf("[INFO] "f_"\n", __VA_ARGS__)

static u32 file_line = 0;

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
    u8 opcode;
    operand operand;
    u8 immediate_16;
} mnemonic;

typedef struct {
    char *names[2]; // Some instructions have aliases like lda = ldaa
    u8 name_count;
    u8 codes[OPERAND_TYPE_COUNT];
    void (*func[OPERAND_TYPE_COUNT]) (cpu *cpu);
    operand_type operands[OPERAND_TYPE_COUNT];
    // The maximum value an operand in immediate addressing mode can have,
    // certain isntruction like 'LDA' can go up to 0xFF but others like 'LDS' can go up to 0xFFFF
    u16 immediate_16;
} instruction;


const char *directives_name[] = { "org", "equ" };
#define DIRECTIVE_COUNT ((u8)(sizeof(directives_name) / sizeof(directives_name[0])))



void (*instr_func[0x100]) (cpu *cpu) = {0};

/*****************************
*        Instructions        *
*****************************/
u16 join(u8 a, u8 b) {
    return (a << 8) | b;
}

// Returns addr and addr + 1 in a single value
u16 join_addr(cpu *cpu, u8 addr) {
    return join(cpu->memory[addr], cpu->memory[addr + 1]);
}

u8 NEXT8(cpu *cpu) {
    return cpu->memory[++cpu->pc];
}

u16 NEXT16(cpu *cpu) {
    u8 b0 = NEXT8(cpu); // high order byte
    u8 b1 = NEXT8(cpu); // low order byte
    return join(b0, b1);
}

u8 DIR_WORD(cpu *cpu) {
    // Get value of the next operand
    u8 addr = NEXT8(cpu);
    // Get value the operand is poiting at
    return cpu->memory[addr];
}

u16 DIR_WORD16(cpu *cpu) {
    // Get values of the next operands
    u8 addr = NEXT8(cpu);
    // Get value the operand is poiting at
    return join_addr(cpu, addr);
}

u8 EXT_WORD(cpu *cpu) {
    return cpu->memory[NEXT16(cpu)];
}

u16 EXT_WORD16(cpu *cpu) {
    u16 addr = NEXT16(cpu);
    return join_addr(cpu, addr);
}

u8 STACK_POP8(cpu *cpu) {
    cpu->sp++;
    return cpu->memory[cpu->sp];
}

u16 STACK_POP16(cpu *cpu) {
    u8 v1 = STACK_POP8(cpu);
    u8 v2 = STACK_POP8(cpu);
    return join(v1, v2);
}

void STACK_PUSH8(cpu *cpu, u8 v) {
    cpu->memory[cpu->sp] = v & 0xFF;
    cpu->sp--;
}

void STACK_PUSH16(cpu *cpu, u16 v) {
    STACK_PUSH8(cpu, v & 0xFF);
    STACK_PUSH8(cpu, (v >> 8) & 0xFF);
}

void SET_FLAGS(cpu *cpu, i16 result, u8 flags) {
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

u16 READ_FROM_PORTS(cpu *cpu, u16 addr) {
    if (addr == PORTA_ADDR) {
        u8 ret = cpu->ports[PORTA] & 0x7;
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

    // If we get there, it means we're not reading from a port
    return 0xFFFF;
}

void SET_LD_FLAGS(cpu *cpu, u8 result) {
    cpu->n = (result >> 7) & 1;
    cpu->z = result == 0;
    cpu->v = 0;
}

void INST_LDA_IMM(cpu *cpu) {
    // Operand is a constant, just load the next byte in memory
    u8 value = NEXT8(cpu);
    cpu->a = value;
    SET_LD_FLAGS(cpu, value);
}

void INST_LDA_DIR(cpu *cpu) {
    u8 value = DIR_WORD(cpu);
    cpu->a = value;
    SET_LD_FLAGS(cpu, value);
}

void INST_LDA_EXT(cpu *cpu) {
    u16 addr = NEXT16(cpu);
    u16 port_val = READ_FROM_PORTS(cpu, addr);
    if (port_val < 0xFF) { // < 0xFF means reading from a port
        cpu->a = port_val;
        SET_LD_FLAGS(cpu, port_val);
    } else {
        u8 value = cpu->memory[addr];
        cpu->a = value;
        SET_LD_FLAGS(cpu, value);
    }
}

void INST_LDB_IMM(cpu *cpu) {
    // Operand is a constant, just load the next byte in memory
    u8 value = NEXT8(cpu);
    cpu->b = value;
    SET_LD_FLAGS(cpu, value);
}

void INST_LDB_DIR(cpu *cpu) {
    u8 value = DIR_WORD(cpu);
    cpu->b = value;
    SET_LD_FLAGS(cpu, value);
}

void INST_LDB_EXT(cpu *cpu) {
    u16 addr = NEXT16(cpu);
    u16 port_val = READ_FROM_PORTS(cpu, addr);
    if (port_val < 0xFF) { // < 0xFF means reading from a port
        cpu->b = port_val;
        SET_LD_FLAGS(cpu, port_val);
    } else {
        u8 value = cpu->memory[addr];
        cpu->b = value;
        SET_LD_FLAGS(cpu, value);
    }
}

void INST_LDD_IMM(cpu *cpu) {
    // Operand is a constant, just load the next byte in memory
    u16 value = NEXT16(cpu);
    cpu->d = value;
    SET_LD_FLAGS(cpu, value);
}

void INST_LDD_DIR(cpu *cpu) {
    u16 value = DIR_WORD(cpu);
    cpu->d = value;
    SET_LD_FLAGS(cpu, value);
}

void INST_LDD_EXT(cpu *cpu) {
    u16 addr = NEXT16(cpu);
    u16 port_val = READ_FROM_PORTS(cpu, addr);
    if (port_val < 0xFF) { // < 0xFF means reading from a port
        cpu->d = port_val;
        SET_LD_FLAGS(cpu, port_val);
    } else {
        u16 value = join_addr(cpu, addr);
        cpu->d = value;
        SET_LD_FLAGS(cpu, value);
    }
}

void INST_ABA(cpu *cpu) {
    i16 result = cpu->a + cpu->b;
    cpu->a = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ADCA_IMM(cpu *cpu) {
    u8 v = NEXT8(cpu);
    i16 result = cpu->a + v + cpu->c;
    cpu->a = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ADCA_DIR(cpu *cpu) {
    u8 v = DIR_WORD(cpu);
    i16 result = cpu->a + v + cpu->c;
    cpu->a = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ADCA_EXT(cpu *cpu) {
    u8 v = EXT_WORD(cpu);
    i16 result = cpu->a + v + cpu->c;
    cpu->a = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ADCB_IMM(cpu *cpu) {
    u8 v = NEXT8(cpu);
    i16 result = cpu->b + v + cpu->c;
    cpu->b = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ADCB_DIR(cpu *cpu) {
    u8 v = DIR_WORD(cpu);
    i16 result = cpu->b + v + cpu->c;
    cpu->b = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ADCB_EXT(cpu *cpu) {
    u8 v = EXT_WORD(cpu);
    i16 result = cpu->b + v + cpu->c;
    cpu->b = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ADDA_IMM(cpu *cpu) {
    u8 v = NEXT8(cpu);
    i16 result = cpu->a + v;
    cpu->a = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ADDA_DIR(cpu *cpu) {
    u8 v = DIR_WORD(cpu);
    i16 result = cpu->a + v;
    cpu->a = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ADDA_EXT(cpu *cpu) {
    u8 v = EXT_WORD(cpu);
    i16 result = cpu->a + v;
    cpu->a = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ADDB_IMM(cpu *cpu) {
    u8 v = NEXT8(cpu);
    i16 result = cpu->b + v;
    cpu->b = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ADDB_DIR(cpu *cpu) {
    u8 v = DIR_WORD(cpu);
    i16 result = cpu->b + v;
    cpu->b = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ADDB_EXT(cpu *cpu) {
    u8 v = EXT_WORD(cpu);
    i16 result = cpu->b + v;
    cpu->b = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ADDD_IMM(cpu *cpu) {
    u8 v = NEXT16(cpu);
    int32_t result = cpu->d + v;
    cpu->d = result & 0xFFFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ADDD_DIR(cpu *cpu) {
    u8 v = DIR_WORD(cpu);
    int32_t result = cpu->d + v;
    cpu->d = result & 0xFFFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ADDD_EXT(cpu *cpu) {
    u16 addr = NEXT16(cpu);
    u16 v = join_addr(cpu, addr);
    int32_t result = cpu->d + v;
    cpu->d = result & 0xFFFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ANDA_IMM(cpu *cpu) {
    u8 v = NEXT8(cpu);
    i8 result = cpu->a & v;
    cpu->a = result;
    SET_FLAGS(cpu, result, ZERO | NEG);
    cpu->v = 0;
}

void INST_ANDA_DIR(cpu *cpu) {
    u8 v = DIR_WORD(cpu);
    i8 result = cpu->a & v;
    cpu->a = result;
    SET_FLAGS(cpu, result, ZERO | NEG);
    cpu->v = 0;
}

void INST_ANDA_EXT(cpu *cpu) {
    u8 v = EXT_WORD(cpu);
    i8 result = cpu->a & v;
    cpu->a = result;
    SET_FLAGS(cpu, result, ZERO | NEG);
    cpu->v = 0;
}

void INST_ANDB_IMM(cpu *cpu) {
    u8 v = NEXT8(cpu);
    i8 result = cpu->b & v;
    cpu->b = result;
    SET_FLAGS(cpu, result, ZERO | NEG);
    cpu->v = 0;
}

void INST_ANDB_DIR(cpu *cpu) {
    u8 v = DIR_WORD(cpu);
    i8 result = cpu->b & v;
    cpu->b = result;
    SET_FLAGS(cpu, result, ZERO | NEG);
    cpu->v = 0;
}

void INST_ANDB_EXT(cpu *cpu) {
    u8 v = EXT_WORD(cpu);
    i8 result = cpu->b & v;
    cpu->b = result;
    SET_FLAGS(cpu, result, ZERO | NEG);
    cpu->v = 0;
}

void INST_ASL_EXT(cpu *cpu) {
    u16 addr = NEXT16(cpu);
    u16 v = cpu->memory[addr];
    u32 result = (v << 1);
    cpu->memory[addr] = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ASLA_INH(cpu *cpu) {
    u16 result = cpu->a << 1;
    cpu->a = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ASLB_INH(cpu *cpu) {
    u16 result = cpu->b << 1;
    cpu->b = result & 0xFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ASLD_INH(cpu *cpu) {
    u32 result = cpu->d << 1;
    cpu->d = result & 0xFFFF;
    SET_FLAGS(cpu, result, CARRY | OFLOW | ZERO | NEG);
}

void INST_ASRA_INH(cpu *cpu) {
    u8 msb = (cpu->a >> 7) & 1;
    u32 result = cpu->a >> 1;
    result |= msb << 7;
    cpu->a = result;

    cpu->c = result & 1;
    SET_FLAGS(cpu, result, ZERO | NEG);
    cpu->v = cpu->n ^ cpu->c;
}

void INST_ASRB_INH(cpu *cpu) {
    u8 msb = (cpu->b >> 7) & 1;
    u32 result = cpu->b >> 1;
    result |= msb << 7;
    cpu->b = result;

    cpu->c = result & 1;
    SET_FLAGS(cpu, result, ZERO | NEG);
    cpu->v = cpu->n ^ cpu->c;
}

void INST_ASR_EXT(cpu *cpu) {
    u16 addr = NEXT16(cpu);
    u8 m = cpu->memory[addr];

    u8 msb = (m >> 7) & 1;
    u32 result = m >> 1;
    result |= msb << 7;
    cpu->memory[addr] = result;

    cpu->c = result & 1;
    SET_FLAGS(cpu, result, ZERO | NEG);
    cpu->v = cpu->n ^ cpu->c;
}

u8 WRITE_TO_PORTS(cpu *cpu, u16 addr) {
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

void INST_STA_DIR(cpu *cpu) {
    u8 addr = NEXT8(cpu);
    cpu->memory[addr] = cpu->a;
    SET_FLAGS(cpu, cpu->a, ZERO | NEG);
    cpu->v = 0;
}


void INST_STA_EXT(cpu *cpu) {
    u16 addr = NEXT16(cpu);
    if (!WRITE_TO_PORTS(cpu, addr)) {
        cpu->memory[addr] = cpu->a;
    }
    SET_FLAGS(cpu, cpu->a, ZERO | NEG);
    cpu->v = 0;
}

void INST_STB_DIR(cpu *cpu) {
    u8 addr = NEXT8(cpu);
    cpu->memory[addr] = cpu->b;
    SET_FLAGS(cpu, cpu->a, ZERO | NEG);
    cpu->v = 0;
}

void INST_STB_EXT(cpu *cpu) {
    u16 addr = NEXT16(cpu);
    if (!WRITE_TO_PORTS(cpu, addr)) {
        cpu->memory[addr] = cpu->b;
    }
    SET_FLAGS(cpu, cpu->a, ZERO | NEG);
    cpu->v = 0;
}

void INST_STD_DIR(cpu *cpu) {
    u8 addr = NEXT8(cpu);
    if (!WRITE_TO_PORTS(cpu, addr)) {
        cpu->memory[addr + 0] = cpu->a;
        cpu->memory[addr + 1] = cpu->b;
    }
    SET_FLAGS(cpu, cpu->a, ZERO | NEG);
    cpu->v = 0;
}

void INST_STD_EXT(cpu *cpu) {
    u16 addr = NEXT16(cpu);
    if (!WRITE_TO_PORTS(cpu, addr)) {
        cpu->memory[addr + 0] = cpu->a;
        cpu->memory[addr + 1] = cpu->b;
    }
    SET_FLAGS(cpu, cpu->a, ZERO | NEG);
    cpu->v = 0;
}

void INST_BRA(cpu *cpu) {
    u8 jmp = NEXT8(cpu);
    cpu->pc += (i8) jmp;
}

void INST_BCC(cpu *cpu) {
    u8 jmp = NEXT8(cpu);
    if (cpu->c == 0) {
        cpu->pc += (i8) jmp;
    }
}

void INST_BCS(cpu *cpu) {
    u8 jmp = NEXT8(cpu);
    if (cpu->c == 1) {
        cpu->pc += (i8) jmp;
    }
}

void INST_BEQ(cpu *cpu) {
    u8 jmp = NEXT8(cpu);
    if (cpu->z == 0) {
        cpu->pc += (i8) jmp;
    }
}

void INST_BGE(cpu *cpu) {
    u8 jmp = NEXT8(cpu);
    if ((cpu->n ^ cpu->v) == 0) {
        cpu->pc += (i8) jmp;
    }
}

void INST_BGT(cpu *cpu) {
    u8 jmp = NEXT8(cpu);
    if (cpu->z + (cpu->n ^ cpu->v) == 0) {
        cpu->pc += (i8) jmp;
    }
}

void INST_BHI(cpu *cpu) {
    u8 jmp = NEXT8(cpu);
    if (cpu->c + cpu->z == 0) {
        cpu->pc += (i8) jmp;
    }
}

void INST_BLE(cpu *cpu) {
    u8 jmp = NEXT8(cpu);
    if (cpu->z + (cpu->n ^ cpu->c) != 0) {
        cpu->pc += (i8) jmp;
    }
}

void INST_BLS(cpu *cpu) {
    u8 jmp = NEXT8(cpu);
    if (cpu->c + cpu->z != 0) {
        cpu->pc += (i8) jmp;
    }
}

void INST_BLT(cpu *cpu) {
    u8 jmp = NEXT8(cpu);
    if (cpu->n + cpu->v != 0) {
        cpu->pc += (i8) jmp;
    }
}

void INST_BMI(cpu *cpu) {
    u8 jmp = NEXT8(cpu);
    if (cpu->n == 1) {
        cpu->pc += (i8) jmp;
    }
}

void INST_BNE(cpu *cpu) {
    u8 jmp = NEXT8(cpu);
    if (cpu->z == 0) {
        cpu->pc += (i8) jmp;
    }
}

void INST_BPL(cpu *cpu) {
    u8 jmp = NEXT8(cpu);
    if (cpu->n == 0) {
        cpu->pc += (i8) jmp;
    }
}

void INST_BRN(cpu *cpu) {
    u8 jmp = NEXT8(cpu);
    (void) jmp;
}

void INST_BVC(cpu *cpu) {
    u8 jmp = NEXT8(cpu);
    if (cpu->v == 0) {
        cpu->pc += (i8) jmp;
    }
}

void INST_BVS(cpu *cpu) {
    u8 jmp = NEXT8(cpu);
    if (cpu->v == 1) {
        cpu->pc += (i8) jmp;
    }
}

void INST_BSR_REL(cpu *cpu) {
    cpu->pc += 2;
    STACK_PUSH16(cpu, cpu->pc);
    cpu->pc += (i8) NEXT8(cpu);
}

void INST_TAB_INH(cpu *cpu) {
    cpu->b = cpu->a;
    SET_FLAGS(cpu, cpu->b, ZERO | NEG);
    cpu->v = 0;
}

void INST_TAP_INH(cpu *cpu) {
    cpu->status = cpu->a;
}

void INST_TBA_INH(cpu *cpu) {
    cpu->a = cpu->b;
    SET_FLAGS(cpu, cpu->b, ZERO | NEG);
    cpu->v = 0;
}

void SET_CMP_FLAGS(cpu *cpu, u8 a, u8 v) {
    i16 r = a - v;
    SET_FLAGS(cpu, r, CARRY | OFLOW | ZERO | NEG);
}

void INST_CMPA_IMM(cpu *cpu) {
    u8 a = cpu->a;
    u8 v = NEXT8(cpu);
    SET_CMP_FLAGS(cpu, a, v);
}

void INST_CMPA_DIR(cpu *cpu) {
    u8 a = cpu->a;
    u8 v = DIR_WORD(cpu);
    SET_CMP_FLAGS(cpu, a, v);
}

void INST_CMPA_EXT(cpu *cpu) {
    u8 a = cpu->a;
    u8 v = EXT_WORD(cpu);
    SET_CMP_FLAGS(cpu, a, v);
}

void INST_CMPB_IMM(cpu *cpu) {
    u8 b = cpu->b;
    u8 v = NEXT8(cpu);
    SET_CMP_FLAGS(cpu, b, v);
}

void INST_CMPB_DIR(cpu *cpu) {
    u8 b = cpu->b;
    u8 v = DIR_WORD(cpu);
    SET_CMP_FLAGS(cpu, b, v);
}

void INST_CMPB_EXT(cpu *cpu) {
    u8 b = cpu->b;
    u8 v = EXT_WORD(cpu);
    SET_CMP_FLAGS(cpu, b, v);
}

void INST_CBA_INH(cpu *cpu) {
    u8 a = cpu->a;
    u8 b = cpu->b;
    SET_CMP_FLAGS(cpu, a, b);
}

void INST_COM_EXT(cpu *cpu) {
    u16 addr = NEXT16(cpu);
    u8 v = cpu->memory[addr];
    v = 0xFF - v;
    cpu->memory[addr] = v & 0xFF;

    SET_FLAGS(cpu, v, NEG | ZERO);
    cpu->v = 0;
    cpu->c = 0;
}

void INST_COMA_INH(cpu *cpu) {
    u8 v = cpu->a;
    v = 0xFF - v;
    cpu->a = v & 0xFF;

    SET_FLAGS(cpu, v, NEG | ZERO);
    cpu->v = 0;
    cpu->c = 0;
}

void INST_COMB_INH(cpu *cpu) {
    u8 v = cpu->b;
    v = 0xFF - v;
    cpu->b = v & 0xFF;

    SET_FLAGS(cpu, v, NEG | ZERO);
    cpu->v = 0;
    cpu->c = 0;
}


void INST_LDS_IMM(cpu *cpu) {
    u16 v = NEXT16(cpu);
    SET_FLAGS(cpu, v, NEG | ZERO);
    cpu->v = 0;
    cpu->sp = v;
}

void INST_LDS_DIR(cpu *cpu) {
    u16 v = DIR_WORD(cpu);
    SET_FLAGS(cpu, v, NEG | ZERO);
    cpu->v = 0;
    cpu->sp = v;
}

void INST_LDS_EXT(cpu *cpu) {
    u16 v = EXT_WORD(cpu);
    SET_FLAGS(cpu, v, NEG | ZERO);
    cpu->v = 0;
    cpu->sp = v;
}

static inline void SET_SHIFT_FLAGS(cpu *cpu, u32 res, u16 base, u16 offset) {
    cpu->n = (res >> offset) & 1;
    cpu->z = res == 0;
    cpu->c = (base >> offset) & 1;
    cpu->v = cpu->n ^ cpu->c;
}

void INST_LSL_EXT(cpu *cpu) {
    u16 addr = NEXT16(cpu);
    u8 v = cpu->memory[addr];
    u16 res = v << 1;
    SET_SHIFT_FLAGS(cpu, res, v, 7);
    cpu->memory[addr] = res & 0xFF;
}

void INST_LSLA_INH(cpu *cpu) {
    u16 res = cpu->a << 1;
    SET_SHIFT_FLAGS(cpu, res, cpu->a, 7);
    cpu->a = res & 0xFF;
}

void INST_LSLB_INH(cpu *cpu) {
    u16 res = cpu->b << 1;
    SET_SHIFT_FLAGS(cpu, res, cpu->b, 7);
    cpu->b = res & 0xFF;
}

void INST_LSLD_INH(cpu *cpu) {
    u32 res = cpu->d << 1;
    SET_SHIFT_FLAGS(cpu, res, cpu->b, 15);
    cpu->d = res & 0xFFFF;
}

void INST_LSR_EXT(cpu *cpu) {
    u16 addr = NEXT16(cpu);
    u8 v = cpu->memory[addr];
    u16 res = v >> 1;
    SET_SHIFT_FLAGS(cpu, res, v, 0);
    cpu->memory[addr] = res & 0xFF;
}

void INST_LSRA_INH(cpu *cpu) {
    u16 res = cpu->a >> 1;
    SET_SHIFT_FLAGS(cpu, res, cpu->a, 0);
    cpu->a = res & 0xFF;
}

void INST_LSRB_INH(cpu *cpu) {
    u16 res = cpu->b >> 1;
    SET_SHIFT_FLAGS(cpu, res, cpu->b, 0);
    cpu->b = res & 0xFF;
}

void INST_LSRD_INH(cpu *cpu) {
    u32 res = cpu->d >> 1;
    SET_SHIFT_FLAGS(cpu, res, cpu->d, 0);
    cpu->d = res & 0xFFFF;
}

void INST_ROL_EXT(cpu *cpu) {
    u16 addr = NEXT16(cpu);
    u8 v = cpu->memory[addr];
    u16 res = (v << 1) | cpu->c;
    SET_SHIFT_FLAGS(cpu, res, v, 7);
    cpu->memory[addr] = res & 0xFF;
}

void INST_ROLA_INH(cpu *cpu) {
    u16 res = (cpu->a << 1) | cpu->c;
    SET_SHIFT_FLAGS(cpu, res, cpu->a, 7);
    cpu->a = res & 0xFF;
}

void INST_ROLB_INH(cpu *cpu) {
    u16 res = (cpu->b << 1) | cpu->c;
    SET_SHIFT_FLAGS(cpu, res, cpu->b, 7);
    cpu->b = res & 0xFF;
}

void INST_ROR_EXT(cpu *cpu) {
    u16 addr = NEXT16(cpu);
    u8 v = cpu->memory[addr];
    u16 res = (v >> 1) | (cpu->c << 7);
    SET_SHIFT_FLAGS(cpu, res, v, 0);
    cpu->memory[addr] = res & 0xFF;
}

void INST_RORA_INH(cpu *cpu) {
    u16 res = (cpu->a >> 1) | (cpu->c << 7);
    SET_SHIFT_FLAGS(cpu, res, cpu->a, 0);
    cpu->a = res & 0xFF;
}

void INST_RORB_INH(cpu *cpu) {
    u16 res = (cpu->b >> 1) | (cpu->c << 7);
    SET_SHIFT_FLAGS(cpu, res, cpu->b, 0);
    cpu->b = res & 0xFF;
}

void INST_RTS_INH(cpu *cpu) {
    u16 ret_addr = STACK_POP16(cpu);
    cpu->pc = ret_addr;
}

void INST_JSR_DIR(cpu *cpu) {
    u8 sub_addr = DIR_WORD(cpu);
    STACK_PUSH16(cpu, cpu->pc);
    cpu->pc = sub_addr;
}

void INST_JSR_EXT(cpu *cpu) {
    u16 sub_addr = NEXT16(cpu);
    STACK_PUSH16(cpu, cpu->pc);
    cpu->pc = sub_addr;
}

void INST_PSHA_INH(cpu *cpu) {
    u8 a = cpu->a;
    STACK_PUSH8(cpu, a);
}

void INST_PSHB_INH(cpu *cpu) {
    u8 b = cpu->b;
    STACK_PUSH8(cpu, b);
}

void INST_PSHX_INH(cpu *cpu) {
    u8 x = cpu->x;
    STACK_PUSH16(cpu, x);
}

void INST_PULA_INH(cpu *cpu) {
    u8 v = STACK_POP8(cpu);
    cpu->a = v;
}

void INST_PULB_INH(cpu *cpu) {
    u8 v = STACK_POP8(cpu);
    cpu->b = v;
}

void INST_PULX_INH(cpu *cpu) {
    u16 v = STACK_POP16(cpu);
    cpu->x = v;
}

void INST_DEC_EXT(cpu *cpu) {
    u16 addr = NEXT16(cpu);
    u8 m = cpu->memory[addr];
    m--;
    cpu->memory[addr] = m & 0xFF;
    SET_FLAGS(cpu, m, OFLOW | ZERO | NEG);
}

void INST_DECA_INH(cpu *cpu) {
    u8 a = cpu->a;
    a--;
    cpu->a = a & 0xFF;
    SET_FLAGS(cpu, a, OFLOW | ZERO | NEG);
}

void INST_DECB_INH(cpu *cpu) {
    u8 b = cpu->b;
    b--;
    cpu->b = b;
    SET_FLAGS(cpu, b, OFLOW | ZERO | NEG);
}

void INST_DES_INH(cpu *cpu) {
    cpu->sp--;
}

void INST_INC_EXT(cpu *cpu) {
    u16 addr = NEXT16(cpu);
    u16 v = cpu->memory[addr] + 1;
    cpu->memory[addr] = v & 0xFF;
    SET_FLAGS(cpu, v, OFLOW | ZERO | NEG);
}

void INST_INCA_INH(cpu *cpu) {
    i16 result = cpu->a + 1;
    cpu->a = result & 0xFF;
    SET_FLAGS(cpu, result, OFLOW | ZERO | NEG);
}

void INST_INCB_INH(cpu *cpu) {
    i16 result = cpu->b + 1;
    cpu->b = result & 0xFF;
    SET_FLAGS(cpu, result, OFLOW | ZERO | NEG);
}

void INST_NEG_EXT(cpu *cpu) {
    u16 addr = NEXT16(cpu);
    u8 v = cpu->memory[addr];
    v = -v;
    cpu->memory[addr] = v;
    SET_FLAGS(cpu, v, CARRY | OFLOW | ZERO | NEG);
}

void INST_NEGA_INH(cpu *cpu) {
    u8 v = cpu->a;
    v = -v;
    cpu->a = v;
    SET_FLAGS(cpu, v, CARRY | OFLOW | ZERO | NEG);
}

void INST_NEGB_INH(cpu *cpu) {
    u8 v = cpu->b;
    v = -v;
    cpu->b = v;
    SET_FLAGS(cpu, v, CARRY | OFLOW | ZERO | NEG);
}

void INST_NOP_INH(cpu *cpu) {
    (void) cpu;
    // DOES NOTHING
}

void INST_ORAA_IMM(cpu *cpu) {
    u8 v = NEXT8(cpu);
    u8 result = cpu->a | v;
    cpu->a = result;
    SET_FLAGS(cpu, v, NEG | ZERO);
    cpu->v = 0;
}

void INST_ORAA_DIR(cpu *cpu) {
    u8 v = DIR_WORD(cpu);
    u8 result = cpu->a | v;
    cpu->a = result;
    SET_FLAGS(cpu, v, NEG | ZERO);
    cpu->v = 0;
}

void INST_ORAA_EXT(cpu *cpu) {
    u8 v = EXT_WORD(cpu);
    u8 result = cpu->a | v;
    cpu->a = result;
    SET_FLAGS(cpu, v, NEG | ZERO);
    cpu->v = 0;
}

void INST_ORAB_IMM(cpu *cpu) {
    u8 v = NEXT8(cpu);
    u8 result = cpu->b | v;
    cpu->b = result;
    SET_FLAGS(cpu, v, NEG | ZERO);
    cpu->v = 0;
}

void INST_ORAB_DIR(cpu *cpu) {
    u8 v = DIR_WORD(cpu);
    u8 result = cpu->b | v;
    cpu->b = result;
    SET_FLAGS(cpu, v, NEG | ZERO);
    cpu->v = 0;
}

void INST_ORAB_EXT(cpu *cpu) {
    u8 v = EXT_WORD(cpu);
    u8 result = cpu->b | v;
    cpu->b = result;
    SET_FLAGS(cpu, v, NEG | ZERO);
    cpu->v = 0;
}

void INST_SUBA_IMM(cpu *cpu) {
    u8 v = NEXT8(cpu);
    i16 result = cpu->a - v;
    cpu->a = result & 0xFF;
    SET_FLAGS(cpu, v, CARRY | OFLOW | ZERO | NEG);
}

void INST_SUBA_DIR(cpu *cpu) {
    u8 v = DIR_WORD(cpu);
    i16 result = cpu->a - v;
    cpu->a = result & 0xFF;
    SET_FLAGS(cpu, v, CARRY | OFLOW | ZERO | NEG);
}

void INST_SUBA_EXT(cpu *cpu) {
    u8 v = EXT_WORD(cpu);
    i16 result = cpu->a - v;
    cpu->a = result & 0xFF;
    SET_FLAGS(cpu, v, CARRY | OFLOW | ZERO | NEG);
}

void INST_SUBB_IMM(cpu *cpu) {
    u8 v = NEXT8(cpu);
    i16 result = cpu->b - v;
    cpu->b = result & 0xFF;
    SET_FLAGS(cpu, v, CARRY | OFLOW | ZERO | NEG);
}

void INST_SUBB_DIR(cpu *cpu) {
    u8 v = DIR_WORD(cpu);
    i16 result = cpu->b - v;
    cpu->b = result & 0xFF;
    SET_FLAGS(cpu, v, CARRY | OFLOW | ZERO | NEG);
}

void INST_SUBB_EXT(cpu *cpu) {
    u8 v = EXT_WORD(cpu);
    i16 result = cpu->b - v;
    cpu->b = result & 0xFF;
    SET_FLAGS(cpu, v, CARRY | OFLOW | ZERO | NEG);
}

void INST_SUBD_IMM(cpu *cpu) {
    u16 v = NEXT16(cpu);
    int32_t result = cpu->d - v;
    cpu->d = result & 0xFFFF;
    SET_FLAGS(cpu, v, CARRY | OFLOW | ZERO | NEG);
}

void INST_SUBD_DIR(cpu *cpu) {
    u16 v = DIR_WORD16(cpu);
    int32_t result = cpu->d - v;
    cpu->d = result & 0xFFFF;
    SET_FLAGS(cpu, v, CARRY | OFLOW | ZERO | NEG);
}

void INST_SUBD_EXT(cpu *cpu) {
    u16 v = EXT_WORD16(cpu);
    int32_t result = cpu->d - v;
    cpu->d = result & 0xFFFF;
    SET_FLAGS(cpu, v, CARRY | OFLOW | ZERO | NEG);
}

void INST_CLR_EXT(cpu *cpu) {
    u16 addr = NEXT16(cpu);
    cpu->memory[addr] = 0;

    u8 top = ((cpu->status >> 4) & 0xF) << 4;
    u8 bot = 0x4; // 0b0100
    cpu->status = top | bot;
}

void INST_CLRA_INH(cpu *cpu) {
    cpu->a = 0;

    u8 top = ((cpu->status >> 4) & 0xF) << 4;
    u8 bot = 0x4; // 0b0100
    cpu->status = top | bot;
}

void INST_CLRB_INH(cpu *cpu) {
    cpu->b = 0;

    u8 top = ((cpu->status >> 4) & 0xF) << 4;
    u8 bot = 0x4; // 0b0100
    cpu->status = top | bot;
}

void INST_CLV_INH(cpu *cpu) {
    cpu->v = 0;
}

void INST_JMP_EXT(cpu *cpu) {
    cpu->pc = EXT_WORD16(cpu);
}

void INST_MUL_INH(cpu *cpu) {
    u32 result = cpu->a * cpu->b;
    cpu->d = result & 0xFFFF;
    SET_FLAGS(cpu, result, CARRY);
}

void INST_STS_DIR(cpu *cpu) {
    u32 result = cpu->sp;
    u8 addr = NEXT8(cpu);
    cpu->memory[addr]     = (cpu->sp >> 8) & 0xFF;
    cpu->memory[addr + 1] = cpu->sp & 0xFF;
    SET_FLAGS(cpu, result, NEG | ZERO);
    cpu->v = 0;
}

void INST_STS_EXT(cpu *cpu) {
    u32 result = cpu->sp;
    u16 addr = NEXT16(cpu);
    cpu->memory[addr]     = (cpu->sp >> 8) & 0xFF;
    cpu->memory[addr + 1] = cpu->sp & 0xFF;
    SET_FLAGS(cpu, result, NEG | ZERO);
    cpu->v = 0;
}

void INST_TPA_INH(cpu *cpu) {
    cpu->a = cpu->status;
}

void INST_TST_EXT(cpu *cpu) {
    u8 v = EXT_WORD(cpu);
    SET_FLAGS(cpu, v, NEG | ZERO);
    cpu->v = 0;
    cpu->c = 0;
}

void INST_TSTA_INH(cpu *cpu) {
    u8 v = cpu->a;
    SET_FLAGS(cpu, v, NEG | ZERO);
    cpu->v = 0;
    cpu->c = 0;

}

void INST_TSTB_INH(cpu *cpu) {
    u8 v = cpu->b;
    SET_FLAGS(cpu, v, NEG | ZERO);
    cpu->v = 0;
    cpu->c = 0;
}

void INST_EORA_IMM(cpu *cpu) {
    u8 a = cpu->a;
    u8 v = NEXT8(cpu);
    u8 result = a ^ v;
    cpu->a = result & 0xFF;
    SET_FLAGS(cpu, v, NEG | ZERO);
    cpu->v = 0;
}

void INST_EORA_DIR(cpu *cpu) {
    u8 a = cpu->a;
    u8 v = DIR_WORD(cpu);
    u8 result = a ^ v;
    cpu->a = result & 0xFF;
    SET_FLAGS(cpu, v, NEG | ZERO);
    cpu->v = 0;
}

void INST_EORA_EXT(cpu *cpu) {
    u8 a = cpu->a;
    u8 v = EXT_WORD(cpu);
    u8 result = a ^ v;
    cpu->a = result & 0xFF;
    SET_FLAGS(cpu, v, NEG | ZERO);
    cpu->v = 0;
}

void INST_EORB_IMM(cpu *cpu) {
    u8 b = cpu->b;
    u8 v = NEXT8(cpu);
    u8 result = b ^ v;
    cpu->b = result & 0xFF;
    SET_FLAGS(cpu, v, NEG | ZERO);
    cpu->v = 0;
}

void INST_EORB_DIR(cpu *cpu) {
    u8 b = cpu->b;
    u8 v = DIR_WORD(cpu);
    u8 result = b ^ v;
    cpu->b = result & 0xFF;
    SET_FLAGS(cpu, v, NEG | ZERO);
    cpu->v = 0;
}

void INST_EORB_EXT(cpu *cpu) {
    u8 b = cpu->b;
    u8 v = EXT_WORD(cpu);
    u8 result = b ^ v;
    cpu->b = result & 0xFF;
    SET_FLAGS(cpu, v, NEG | ZERO);
    cpu->v = 0;
}

void INST_INS_INH(cpu *cpu) {
    cpu->sp++;
}

void INST_SBA_INH(cpu *cpu) {
    u8 a = cpu->a;
    u8 b = cpu->b;
    SET_CMP_FLAGS(cpu, a, b);
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
        .names = {"ldad", "ldd"}, .name_count = 2,
        .codes = {[IMMEDIATE]=0xCC, [DIRECT]=0xDC, [EXTENDED]=0xFC},
        .func =  {
            [IMMEDIATE]=INST_LDD_IMM,
            [DIRECT]=INST_LDD_DIR,
            [EXTENDED]=INST_LDD_EXT,
        },
        .operands = { IMMEDIATE, EXTENDED, DIRECT },
        .immediate_16 = 1,
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
        .names = {"std"}, .name_count = 1,
        .codes = {[DIRECT]=0xDD, [EXTENDED]=0xFD},
        .func = {
            [DIRECT]=INST_STD_DIR,
            [EXTENDED]=INST_STD_EXT,
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
        .operands = { IMMEDIATE, EXTENDED, DIRECT },
    },
    {
        .names = {"addd"}, .name_count = 1,
        .codes = {[IMMEDIATE]=0xC3, [DIRECT]=0xD3, [EXTENDED]=0xF3},
        .func =  {
            [IMMEDIATE]=INST_ADDD_IMM,
            [DIRECT]=INST_ADDD_DIR,
            [EXTENDED]=INST_ADDD_EXT
        },
        .operands = { IMMEDIATE, EXTENDED, DIRECT },
        .immediate_16 = 1
    },
    {
        .names = {"anda"}, .name_count = 1,
        .codes = {[IMMEDIATE]=0x84, [DIRECT]=0x94, [EXTENDED]=0xB4},
        .func =  {
            [IMMEDIATE]=INST_ANDA_IMM,
            [DIRECT]=INST_ANDA_DIR,
            [EXTENDED]=INST_ANDA_EXT
        },
        .operands = { IMMEDIATE, EXTENDED, DIRECT },
    },
    {
        .names = {"andb"}, .name_count = 1,
        .codes = {[IMMEDIATE]=0xC4, [DIRECT]=0xD4, [EXTENDED]=0xF4},
        .func =  {
            [IMMEDIATE]=INST_ANDB_IMM,
            [DIRECT]=INST_ANDB_DIR,
            [EXTENDED]=INST_ANDB_EXT
        },
        .operands = { IMMEDIATE, EXTENDED, DIRECT },
    },
    {
        .names = {"asl"}, .name_count = 1,
        .codes = {[EXTENDED]=0x78},
        .func =  {[EXTENDED]=INST_ASL_EXT},
        .operands = { EXTENDED }
    },
    {
        .names = {"asla"}, .name_count = 1,
        .codes = {[INHERENT]=0x48},
        .func =  {[INHERENT]=INST_ASLA_INH},
        .operands = { INHERENT }
    },
    {
        .names = {"aslb"}, .name_count = 1,
        .codes = {[INHERENT]=0x58},
        .func =  {[INHERENT]=INST_ASLB_INH},
        .operands = { INHERENT }
    },
    {
        .names = {"asld"}, .name_count = 1,
        .codes = {[INHERENT]=0x05},
        .func =  {[INHERENT]=INST_ASLD_INH},
        .operands = { INHERENT }
    },
    {
        .names = {"asr"}, .name_count = 1,
        .codes = {[EXTENDED]=0x77},
        .func =  {[EXTENDED]=INST_ASR_EXT},
        .operands = { EXTENDED }
    },
    {
        .names = {"asra"}, .name_count = 1,
        .codes = {[INHERENT]=0x47},
        .func =  {[INHERENT]=INST_ASRA_INH},
        .operands = { INHERENT }
    },
    {
        .names = {"asrb"}, .name_count = 1,
        .codes = {[INHERENT]=0x57},
        .func =  {[INHERENT]=INST_ASRB_INH},
        .operands = { INHERENT }
    },
    {
        .names = {"tab"}, .name_count = 1,
        .codes = {[INHERENT]=0x16},
        .func = {[INHERENT]=INST_TAB_INH},
        .operands = { INHERENT }
    },
    {
        .names = {"tap"}, .name_count = 1,
        .codes = {[INHERENT]=0x06},
        .func = {[INHERENT]=INST_TAP_INH},
        .operands = { INHERENT }
    },
    {
        .names = {"tba"}, .name_count = 1,
        .codes = {[INHERENT]=0x17},
        .func = {[INHERENT]=INST_TBA_INH},
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
        .names = {"cba"}, .name_count = 1,
        .codes = {[INHERENT]=0x81},
        .func =  {[INHERENT]=INST_CBA_INH},
        .operands = {INHERENT},
    },
    {
        .names = {"com"}, .name_count = 1,
        .codes = {[EXTENDED]=0x73},
        .func =  {[EXTENDED]=INST_COM_EXT},
        .operands = {EXTENDED},
    },
    {
        .names = {"coma"}, .name_count = 1,
        .codes = {[INHERENT]=0x43},
        .func =  {[INHERENT]=INST_COMA_INH},
        .operands = {INHERENT},
    },
    {
        .names = {"comb"}, .name_count = 1,
        .codes = {[INHERENT]=0x53},
        .func =  {[INHERENT]=INST_COMB_INH},
        .operands = {INHERENT},
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
        .names = {"bsr"}, .name_count = 1,
        .codes = {[RELATIVE]=0x8D},
        .func = {[RELATIVE]=INST_BSR_REL},
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
        .names = {"lsl"}, .name_count = 1,
        .codes = {[EXTENDED]=0x78},
        .func = { [EXTENDED]=INST_LSL_EXT },
        .operands = { EXTENDED },
    },
    {
        .names = {"lsla"}, .name_count = 1,
        .codes = {[INHERENT]=0x48},
        .func = { [INHERENT]=INST_LSLA_INH },
        .operands = { INHERENT },
    },
    {
        .names = {"lslb"}, .name_count = 1,
        .codes = {[INHERENT]=0x58},
        .func = { [INHERENT]=INST_LSLB_INH },
        .operands = { INHERENT },
    },
    {
        .names = {"lsld"}, .name_count = 1,
        .codes = {[INHERENT]=0x05},
        .func = { [INHERENT]=INST_LSLD_INH },
        .operands = { INHERENT },
    },
    {
        .names = {"lsr"}, .name_count = 1,
        .codes = {[EXTENDED]=0x74},
        .func = { [EXTENDED]=INST_LSR_EXT },
        .operands = { EXTENDED },
    },
    {
        .names = {"lsra"}, .name_count = 1,
        .codes = {[INHERENT]=0x44},
        .func = { [INHERENT]=INST_LSRA_INH },
        .operands = { INHERENT },
    },
    {
        .names = {"lsrb"}, .name_count = 1,
        .codes = {[INHERENT]=0x54},
        .func = { [INHERENT]=INST_LSRB_INH },
        .operands = { INHERENT },
    },
    {
        .names = {"lsrd"}, .name_count = 1,
        .codes = {[INHERENT]=0x04},
        .func = { [INHERENT]=INST_LSRD_INH },
        .operands = { INHERENT },
    },
    {
        .names = {"rol"}, .name_count = 1,
        .codes = {[EXTENDED]=0x79},
        .func = { [EXTENDED]=INST_ROL_EXT },
        .operands = { EXTENDED },
    },
    {
        .names = {"rola"}, .name_count = 1,
        .codes = {[INHERENT]=0x49},
        .func = { [INHERENT]=INST_ROLA_INH },
        .operands = { INHERENT },
    },
    {
        .names = {"rolb"}, .name_count = 1,
        .codes = {[INHERENT]=0x59},
        .func = { [INHERENT]=INST_ROLB_INH },
        .operands = { INHERENT },
    },
    {
        .names = {"ror"}, .name_count = 1,
        .codes = {[EXTENDED]=0x76},
        .func = { [EXTENDED]=INST_ROR_EXT },
        .operands = { EXTENDED },
    },
    {
        .names = {"rora"}, .name_count = 1,
        .codes = {[INHERENT]=0x46},
        .func = { [INHERENT]=INST_RORA_INH },
        .operands = { INHERENT },
    },
    {
        .names = {"rorb"}, .name_count = 1,
        .codes = {[INHERENT]=0x56},
        .func = { [INHERENT]=INST_RORB_INH },
        .operands = { INHERENT },
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
        .names = {"dec"}, .name_count = 1,
        .codes = {[EXTENDED]=0x7A},
        .func = { [EXTENDED]=INST_DEC_EXT},
        .operands = { EXTENDED },
    },
    {
        .names = {"deca"}, .name_count = 1,
        .codes = {[INHERENT]=0x4A},
        .func = { [INHERENT]=INST_DECA_INH},
        .operands = { INHERENT },
    },
    {
        .names = {"decb"}, .name_count = 1,
        .codes = {[INHERENT]=0x5A},
        .func = { [INHERENT]=INST_DECB_INH},
        .operands = { INHERENT },
    },
    {
        .names = {"des"}, .name_count = 1,
        .codes = {[INHERENT]=0x34},
        .func = { [INHERENT]=INST_DES_INH},
        .operands = { INHERENT },
    },
    {
        .names = {"inc"}, .name_count = 1,
        .codes = {[EXTENDED]=0x7C},
        .func = { [EXTENDED]=INST_INC_EXT},
        .operands = { EXTENDED },
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
    {
        .names = {"neg"}, .name_count = 1,
        .codes = {[EXTENDED]=0x70},
        .func = { [EXTENDED]=INST_NEG_EXT},
        .operands = { EXTENDED },
    },
    {
        .names = {"nega"}, .name_count = 1,
        .codes = {[INHERENT]=0x40},
        .func = { [INHERENT]=INST_NEGA_INH},
        .operands = { INHERENT },
    },
    {
        .names = {"negb"}, .name_count = 1,
        .codes = {[INHERENT]=0x50},
        .func = { [INHERENT]=INST_NEGB_INH},
        .operands = { INHERENT },
    },
    {
        .names = {"nop"}, .name_count = 1,
        .codes = {[INHERENT]=0x01},
        .func = { [INHERENT]=INST_NOP_INH},
        .operands = { INHERENT },
    },
    {
        .names = {"oraa", "ora"}, .name_count = 2,
        .codes = {[IMMEDIATE]=0x8A, [DIRECT]=0x9A, [EXTENDED]=0xBA},
        .func =  {
            [IMMEDIATE]=INST_ORAA_IMM,
            [DIRECT]=INST_ORAA_DIR,
            [EXTENDED]=INST_ORAA_EXT,
        },
        .operands = { IMMEDIATE, EXTENDED, DIRECT },
    },
    {
        .names = {"orab", "orb"}, .name_count = 2,
        .codes = {[IMMEDIATE]=0xCA, [DIRECT]=0xDA, [EXTENDED]=0xFA},
        .func =  {
            [IMMEDIATE]=INST_ORAB_IMM,
            [DIRECT]=INST_ORAB_DIR,
            [EXTENDED]=INST_ORAB_EXT,
        },
        .operands = { IMMEDIATE, EXTENDED, DIRECT },
    },
    {
        .names = {"suba"}, .name_count = 1,
        .codes = {[IMMEDIATE]=0x80, [DIRECT]=0x90, [EXTENDED]=0xB0},
        .func =  {
            [IMMEDIATE]=INST_SUBA_IMM,
            [DIRECT]=INST_SUBA_DIR,
            [EXTENDED]=INST_SUBA_EXT,
        },
        .operands = { IMMEDIATE, EXTENDED, DIRECT },
    },
    {
        .names = {"subb"}, .name_count = 1,
        .codes = {[IMMEDIATE]=0xC0, [DIRECT]=0xD0, [EXTENDED]=0xF0},
        .func =  {
            [IMMEDIATE]=INST_SUBB_IMM,
            [DIRECT]=INST_SUBB_DIR,
            [EXTENDED]=INST_SUBB_EXT,
        },
        .operands = { IMMEDIATE, EXTENDED, DIRECT },
    },
    {
        .names = {"subd"}, .name_count = 1,
        .codes = {[IMMEDIATE]=0x83, [DIRECT]=0x93, [EXTENDED]=0xB3},
        .func =  {
            [IMMEDIATE]=INST_SUBD_IMM,
            [DIRECT]=INST_SUBD_DIR,
            [EXTENDED]=INST_SUBD_EXT,
        },
        .operands = { IMMEDIATE, EXTENDED, DIRECT },
        .immediate_16 = 1
    },
    {
        .names = {"clr"}, .name_count = 1,
        .codes = {[EXTENDED]=0x7F},
        .func =  {[EXTENDED]=INST_CLR_EXT},
        .operands = { EXTENDED },
    },
    {
        .names = {"clra"}, .name_count = 1,
        .codes = {[INHERENT]=0x4F},
        .func =  {[INHERENT]=INST_CLRA_INH},
        .operands = { INHERENT },
    },
    {
        .names = {"clrb"}, .name_count = 1,
        .codes = {[INHERENT]=0x5F},
        .func =  {[INHERENT]=INST_CLRB_INH},
        .operands = { INHERENT },
    },
    {
        .names = {"clv"}, .name_count = 1,
        .codes = {[INHERENT]=0x5F},
        .func =  {[INHERENT]=INST_CLV_INH},
        .operands = { INHERENT },
    },
    {
        .names = {"jmp"}, .name_count = 1,
        .codes = {[EXTENDED]=0x7E},
        .func =  {[EXTENDED]=INST_JMP_EXT},
        .operands = { EXTENDED },
    },
    {
        .names = {"mul"}, .name_count = 1,
        .codes = {[INHERENT]=0x3D},
        .func =  {[INHERENT]=INST_MUL_INH},
        .operands = { INHERENT },
    },
    {
        .names = {"sts"}, .name_count = 1,
        .codes = {[DIRECT]=0x9F, [EXTENDED]=0xBF},
        .func =  {
            [DIRECT]=INST_STS_DIR,
            [EXTENDED]=INST_STS_EXT,
        },
        .operands = { DIRECT, EXTENDED },
    },
    {
        .names = {"tpa"}, .name_count = 1,
        .codes = {[INHERENT]=0x07},
        .func =  {[INHERENT]=INST_TPA_INH},
        .operands = { INHERENT },
    },
    {
        .names = {"tst"}, .name_count = 1,
        .codes = {[EXTENDED]=0x7D},
        .func =  {[EXTENDED]=INST_TST_EXT},
        .operands = { EXTENDED },
    },
    {
        .names = {"tsta"}, .name_count = 1,
        .codes = {[INHERENT]=0x4D},
        .func =  {[INHERENT]=INST_TSTA_INH},
        .operands = { INHERENT },
    },
    {
        .names = {"tst"}, .name_count = 1,
        .codes = {[INHERENT]=0x5D},
        .func =  {[INHERENT]=INST_TSTB_INH},
        .operands = { INHERENT },
    },
    {
        .names = {"tst"}, .name_count = 1,
        .codes = {[EXTENDED]=0x07},
        .func =  {[EXTENDED]=INST_TST_EXT},
        .operands = { EXTENDED },
    },
    {
        .names = {"tst"}, .name_count = 1,
        .codes = {[EXTENDED]=0x07},
        .func =  {[EXTENDED]=INST_TST_EXT},
        .operands = { EXTENDED },
    },
    {
        .names = {"tst"}, .name_count = 1,
        .codes = {[EXTENDED]=0x07},
        .func =  {[EXTENDED]=INST_TST_EXT},
        .operands = { EXTENDED },
    },
    {
        .names = {"tst"}, .name_count = 1,
        .codes = {[EXTENDED]=0x07},
        .func =  {[EXTENDED]=INST_TST_EXT},
        .operands = { EXTENDED },
    },
    {
        .names = {"tst"}, .name_count = 1,
        .codes = {[EXTENDED]=0x07},
        .func =  {[EXTENDED]=INST_TST_EXT},
        .operands = { EXTENDED },
    },
    {
        .names = {"tst"}, .name_count = 1,
        .codes = {[EXTENDED]=0x07},
        .func =  {[EXTENDED]=INST_TST_EXT},
        .operands = { EXTENDED },
    },
    {
        .names = {"tst"}, .name_count = 1,
        .codes = {[EXTENDED]=0x07},
        .func =  {[EXTENDED]=INST_TST_EXT},
        .operands = { EXTENDED },
    },
    {
        .names = {"tst"}, .name_count = 1,
        .codes = {[EXTENDED]=0x07},
        .func =  {[EXTENDED]=INST_TST_EXT},
        .operands = { EXTENDED },
    },
    {
        .names = {"tst"}, .name_count = 1,
        .codes = {[EXTENDED]=0x07},
        .func =  {[EXTENDED]=INST_TST_EXT},
        .operands = { EXTENDED },
    },
    {
        .names = {"tst"}, .name_count = 1,
        .codes = {[EXTENDED]=0x07},
        .func =  {[EXTENDED]=INST_TST_EXT},
        .operands = { EXTENDED },
    },
    {
        .names = {"tst"}, .name_count = 1,
        .codes = {[EXTENDED]=0x07},
        .func =  {[EXTENDED]=INST_TST_EXT},
        .operands = { EXTENDED },
    },
    {
        .names = {"tst"}, .name_count = 1,
        .codes = {[EXTENDED]=0x07},
        .func =  {[EXTENDED]=INST_TST_EXT},
        .operands = { EXTENDED },
    },
    {
        .names = {"tst"}, .name_count = 1,
        .codes = {[EXTENDED]=0x07},
        .func =  {[EXTENDED]=INST_TST_EXT},
        .operands = { EXTENDED },
    },
    {
        .names = {"tst"}, .name_count = 1,
        .codes = {[EXTENDED]=0x07},
        .func =  {[EXTENDED]=INST_TST_EXT},
        .operands = { EXTENDED },
    },
    {
        .names = {"tst"}, .name_count = 1,
        .codes = {[EXTENDED]=0x07},
        .func =  {[EXTENDED]=INST_TST_EXT},
        .operands = { EXTENDED },
    },
    {
        .names = {"tst"}, .name_count = 1,
        .codes = {[EXTENDED]=0x07},
        .func =  {[EXTENDED]=INST_TST_EXT},
        .operands = { EXTENDED },
    },
    {
        .names = {"tst"}, .name_count = 1,
        .codes = {[EXTENDED]=0x07},
        .func =  {[EXTENDED]=INST_TST_EXT},
        .operands = { EXTENDED },
    },
    {
        .names = {"tst"}, .name_count = 1,
        .codes = {[EXTENDED]=0x07},
        .func =  {[EXTENDED]=INST_TST_EXT},
        .operands = { EXTENDED },
    },
    {
        .names = {"tst"}, .name_count = 1,
        .codes = {[EXTENDED]=0x07},
        .func =  {[EXTENDED]=INST_TST_EXT},
        .operands = { EXTENDED },
    },
    {
        .names = {"tst"}, .name_count = 1,
        .codes = {[EXTENDED]=0x07},
        .func =  {[EXTENDED]=INST_TST_EXT},
        .operands = { EXTENDED },
    },
    {
        .names = {"eora"}, .name_count = 1,
        .codes = {[IMMEDIATE]=0x88,[DIRECT]=0x98,[EXTENDED]=0xB8},
        .func =  {
            [IMMEDIATE]=INST_EORA_IMM,
            [DIRECT]=INST_EORA_DIR,
            [EXTENDED]=INST_EORA_EXT
        },
        .operands = { IMMEDIATE, DIRECT, EXTENDED },
    },
    {
        .names = {"eorb"}, .name_count = 1,
        .codes = {[IMMEDIATE]=0xC8,[DIRECT]=0xD8,[EXTENDED]=0xF8},
        .func =  {
            [IMMEDIATE]=INST_EORB_IMM,
            [DIRECT]=INST_EORB_DIR,
            [EXTENDED]=INST_EORB_EXT
        },
        .operands = { IMMEDIATE, DIRECT, EXTENDED },
    },
    {
        .names = {"ins"}, .name_count = 1,
        .codes = {[INHERENT]=0x31 },
        .func =  { [INHERENT]=INST_INS_INH },
        .operands = { INHERENT },
    },
    {
        .names = {"sba"}, .name_count = 1,
        .codes = {[INHERENT]=0x10},
        .func =  { [INHERENT]=INST_SBA_INH },
        .operands = { INHERENT },
    },

};

#define INSTRUCTION_COUNT ((u8)(sizeof(instructions) / sizeof(instructions[0])))


/*****************************
*           Utils            *
*****************************/

void free_cpu(cpu *cpu) {
    for (u8 i = 0; i < cpu->label_count; ++i) {
        free((void *)cpu->labels[i].label);
    }
}

void destroy_cpu(cpu *cpu) {
    free_cpu(cpu);
    free(cpu);
}

void add_instructions_func() {
    memset(instr_func, 0, 0x100 * sizeof(void*));
    for (u8 i = 0; i < INSTRUCTION_COUNT; ++i) {
        instruction *inst = &instructions[i];
        operand_type *type = inst->operands;
        while (*type != NONE) {
            instr_func[inst->codes[*type]] = inst->func[*type];
            type++;
        }
    }
}

u8 str_prefix(const char *str, const char *pre)
{
    return strncmp(pre, str, strlen(pre)) == 0;
}

u8 is_str_in_parts(const char *str, char **parts, u8 parts_count) {
    for (u8 i = 0; i < parts_count; i++) {
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

u8 is_valid_operand_type(instruction *inst, operand_type type) {
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

directive *get_directive_by_label(const char *label, directive *labels, u8 label_count) {
    for (u8 i = 0; i < label_count; ++i) {
        if (strcmp(label, labels[i].label) == 0) {
            return &labels[i];
        }
    }
    return NULL;
}

u8 is_directive(const char *str) {
    for (u8 i = 0; i < DIRECTIVE_COUNT; ++i) {
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
    for (u8 i = 0; *str != '\0'; ++i, ++str) {
        *str = tolower(*str);
    }
}

u8 split_by_space(char *str, char **out, u8 n) {
    assert(str);
    assert(n);
    u8 nb_parts = 0;
    u8 state = 0;
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
    if (str == NULL) return NONE;
    if (str[0] == '#')                  return IMMEDIATE;
    if (str[0] == '<' && str[1] == '$') return DIRECT;
    if (str[0] == '>' && str[1] == '$') return EXTENDED;
    if (str[0] == '$')                  return EXTENDED;
    return NONE;
}

u16 convert_str_from_base(const char *str, u8 base) {
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

u16 hex_str_to_u16(const char *str) {
    str += 2; // skips the #$
    return convert_str_from_base(str, 16);
}

u16 dec_str_to_u16(const char *str) {
    str += 1; // skips the #
    return convert_str_from_base(str, 10);
}

u16 bin_str_to_u16(const char *str) {
    str += 2; // skips the #$
    return convert_str_from_base(str, 2);
}

operand get_operand_value(const char *str, directive *labels, u8 label_count) {
    directive *directive = NULL;
    if (labels) {
        directive = get_directive_by_label(str, labels, label_count);
    }

    if (directive != NULL) {
        return (operand) {directive->operand.value, directive->operand.type, 1};
    }

    u8 offset = str[0] == '<' || str[0] == '>';
    u16 operand_value = 0;
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

mnemonic line_to_mnemonic(char *line, directive *labels, u8 label_count, u16 addr) {
    char *parts[5] = {0};
    u8 nb_parts = split_by_space(line, parts, 5);
    if (nb_parts == 0) {
        return (mnemonic) {0, {0, 0, 0}, 0};
    }

    // If there is only a label
    if (nb_parts == 1 && parts[0] != NULL) {
        return (mnemonic) {0, {0, 0, 0}, 0};
    }

    nb_parts--; // Does as if there was no label

    instruction *inst = opcode_str_to_hex(parts[1]);
    if (inst == NULL) {
        ERROR("%s is an undefined (or not implemented) instruction", parts[1]);
    }

    u8 need_operand = inst->operands[0] != NONE && inst->operands[0] != INHERENT;
    if (need_operand != (nb_parts - 1)) { // need an operand but none were given
        ERROR("%s instruction requires %d operand but %d recieved\n", parts[1], need_operand, nb_parts - 1);
    }

    mnemonic result = {0};
    result.immediate_16 = inst->immediate_16;
    if (need_operand) {
        result.operand = get_operand_value(parts[2], labels, label_count);
        if (inst->operands[0] == RELATIVE) {
            u16 operand_value = result.operand.value;
            if (result.operand.from_label) {
                i8 offset = result.operand.value - addr - 2;
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

u8 add_mnemonic_to_memory(cpu *cpu, mnemonic *m, u16 addr) {
    u8 written = 0; // Number of bytes written
    cpu->memory[addr + (written++)] = m->opcode;
    if (m->operand.type != NONE && m->operand.type != INHERENT) {
        // TODO: Certain instruction such as CPX uses 2 operands even for immediate mode
        if (m->operand.value > 0xFF || m->operand.type == EXTENDED || (m->operand.type == IMMEDIATE && m->immediate_16)) {
            cpu->memory[addr + written++] = (m->operand.value >> 8) & 0xFF;
        }
        cpu->memory[addr + written++] = m->operand.value & 0xFF;
    }
    return written;
}

directive line_to_directive(char *line, directive *labels, u8 label_count) {
    char *parts[5] = {0};
    u8 nb_parts = split_by_space(line, parts, 5);
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

    operand_type type = get_operand_type(parts[2]);
    return (directive) {NULL, parts[1], {0, type, 0}, NOT_A_DIRECTIVE};
}

void load_program(cpu *cpu, const char *file_path) {
    FILE *f = fopen(file_path, "r");
    if (!f) {
        ERROR("Error while opennig file : %s\n", file_path);
    }

    // First PASS
    // On the first we get all the labels and directives which enables the use of labels that are later in the code.

    char buf[100];
    u16 addr = 0x0;
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
            if (d.operand.type == DIRECT || (d.operand.type == IMMEDIATE && !inst->immediate_16)) {
                addr += 1;
            } else if (d.operand.type == EXTENDED || (d.operand.type == IMMEDIATE && inst->immediate_16)) {
                addr += 2;
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
        u8 inst = cpu->memory[cpu->pc];
        if (instr_func[inst] != NULL) {
            (*instr_func[inst])(cpu); // Call the function with this opcode
        }
        cpu->pc++;
    }
}

void init_cpu(cpu *cpu, const char *fn) {
    add_instructions_func();
    set_default_ddr(cpu);
    load_program(cpu, fn);
}

cpu *new_cpu(const char *fn) {
    cpu *c = calloc(1, sizeof(cpu));
    init_cpu(c, fn);
    return c;
}

#endif // EMULATOR_IMPLEMENTATION

