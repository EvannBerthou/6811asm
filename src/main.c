#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

#define MAX_MEMORY (1 << 16)
#define MAX_LABELS 0xFF
#define FMT8 "0x%02x"
#define FMT16 "0x%04x"

const char *strdup(const char *base) {
    size_t len = strlen(base);
    char *str = calloc(len + 1, sizeof(char));
    if (str == NULL) {
        printf("calloc error\n");
        exit(1);
    }
    memcpy(str, base, len);
    return str;
}

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

typedef enum {
    ORG,
    CONSTANT,
    RMB,
    FCC,
    NOT_A_DIRECTIVE,
    DIRECTIVE_TYPE_COUNT
} directive_type;

typedef struct {
    char *names[2]; // Some instructions have aliases like lda = ldaa
    uint8_t name_count;
    uint8_t codes[OPERAND_TYPE_COUNT];
    operand_type operands[OPERAND_TYPE_COUNT];
} instruction;

typedef struct {
    uint16_t value;
    operand_type type;
} operand;

typedef struct {
    uint8_t opcode;
    operand operand;
} mnemonic;

typedef struct {
    const char *label;
    operand operand;
    directive_type type;
} directive;

mnemonic nop_mnemonic = {.opcode = 0x1, .operand = {.value = 0, .type = NONE}};
operand empty_operand = {.value = 0, .type = NONE};
directive empty_directive = {.label = NULL, .operand = {.value = 0, .type = NONE}, .type = NOT_A_DIRECTIVE};

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

const char *directives_name[] = { "org", "equ" };
#define DIRECTIVE_COUNT ((uint8_t)(sizeof(directives_name) / sizeof(directives_name[0])))


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

/*****************************
*          Assembly          *
*****************************/

directive *get_directive_by_label(const char *label, directive *labels, uint8_t label_count) {
    for (uint8_t i = 0; i < label_count; ++i) {
        if (strcmp(label, labels[i].label) == 0) {
            return &labels[i];
        }
    }
    return NULL;
}

uint8_t split_by_space(char *str, char **out, uint8_t n) {
    assert(str);
    assert(n);
    uint8_t nb_parts = 0;
    uint8_t state = 0;
    while (*str) {
        if (*str == ' ' || *str == '\n' || *str == '\t' || *str == '\r') {
            if (state == 1) {
                *str = '\0';
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
operand_type get_operand_type(const char *str, uint16_t value) {
    if (str[0] == '#')                  return IMMEDIATE;
    if (str[0] == '$' && value <= 0xFF) return DIRECT;
    if (str[0] == '$' && value >  0xFF) return EXTENDED;
    return NONE;
}

operand get_operand_value(const char *str, directive *labels, uint8_t label_count) {
    const directive *directive = get_directive_by_label(str, labels, label_count);
    if (directive != NULL) {
        printf("Label found (%s) with value %u\n", directive->label, directive->operand.value);
        return (operand) {directive->operand.value, directive->operand.type};
    }

    // Find where the number starts (skips non digit but keeps hex values)
    const char *number_part = str;
    while (!isdigit(*number_part) && (*number_part < 'a' || *number_part > 'c')) number_part++;
    // Convert to a number
    char *end;
    long l = strtol(number_part, &end, 16);
    if (l == 0 && number_part == end) {
        printf("Error while parsing the operand %s\n", str);
        return empty_operand;
    }
    uint16_t operand_value = l & 0xFFFF; // Keep the value below 0xFFFF
    operand_type type = get_operand_type(str, operand_value);
    if (type == NONE) {
        return empty_operand;
    }
    return (operand) {operand_value, type};
}

// TODO: Check labels array for operands
mnemonic line_to_mnemonic(char *line, directive *labels, uint8_t label_count) {
    char *parts[10] = {0};
    uint8_t nb_parts = split_by_space(line, parts, 10);
    if (nb_parts > 2) {
        printf("Too many operands\n");
        return nop_mnemonic;
    }

    instruction *inst = opcode_str_to_hex(parts[0]);
    if (inst == NULL) {
        printf("%s is an undefined (or not implemented) instruction\n", parts[0]);
        return nop_mnemonic;
    }

    uint8_t need_operand = inst->operands[0] != NONE && inst->operands[0] != INHERENT;
    if (need_operand != (nb_parts - 1)) { // need an operand but none were given
        printf("%s instruction requires %d operand but %d recieved\n", parts[0], need_operand, nb_parts - 1);
        return nop_mnemonic;
    }

    mnemonic result = {0};
    if (need_operand) {
        operand op = get_operand_value(parts[1], labels, label_count);
        if (op.type == NONE) {
            printf("Invalid operand for ");
            for (uint8_t i = 0; i < nb_parts; i++) printf("%s ", parts[i]);
            printf("\n");
            return nop_mnemonic;
        }
        result.operand = op;
    }
    result.opcode = inst->codes[inst->operands[result.operand.type]];
    return result;
}

uint8_t add_mnemonic_to_memory(cpu *cpu, mnemonic *m, uint16_t addr) {
    uint8_t written = 0; // Number of bytes written
    cpu->memory[addr + (written++)] = m->opcode;
    if (m->operand.type != NONE) {
        // Only extended uses 2 bytes for the operand
        // TODO: Certain instruction such as CPX uses 2 operands even for immediate mode
        if (m->operand.type == EXTENDED) {
            cpu->memory[addr + written++] = (m->operand.value >> 8) & 0xFF;
            cpu->memory[addr + written++] = m->operand.value & 0xFF;
        } else {
            cpu->memory[addr + written++] = m->operand.value & 0xFF;
        }
    }
    return written;
}

directive line_to_directive(char *line, directive *labels, uint8_t label_count) {
    if (strstr(line, "equ")) {
        char *parts[3] = {0};
        uint8_t nb_parts = split_by_space(line, parts, 3);
        if (nb_parts != 3) {
            printf("equ format : <LABEL> equ <VALUE>\n");
            return empty_directive;
        }
        operand operand = get_operand_value(parts[2], labels, label_count);
        if (operand.type == NONE) {
            return empty_directive;
        }
        return (directive) {strdup(parts[0]), {operand.value, operand.type}, CONSTANT};
    }
    if (strstr(line, "org")) {
        char *parts[2] = {0};
        uint8_t nb_parts = split_by_space(line, parts, 2);
        if (nb_parts != 2) {
            printf("ORG format : ORG <ADDR> ($<VALUE>)\n");
            return empty_directive;
        }

        operand operand = get_operand_value(parts[1], labels, label_count);
        if (operand.type == NONE) {
            printf("No operand found\n");
            return empty_directive;
        }
        return (directive) {NULL, {operand.value, EXTENDED}, ORG};
    }

    return empty_directive;
}

int str_empty(const char *str) {
    do {
        if (*str != '\0' && *str != ' ' && *str != '\n') return 0;
    } while(*str++ != '\0');
    return 1;
}

int load_program(cpu *cpu, const char *file_path) {
    FILE *f = fopen(file_path, "r");
    if (!f) {
        printf("Error opennig file : %s\n", file_path);
        return 0;
    }

    uint16_t org_program = 0xFFFF;
    directive labels[MAX_LABELS] = {0};
    uint8_t label_count = 0;

    // first pass
    char buf[100];
    for (;;) {
        if (fgets(buf, 100, f) == NULL) {
            break;
        }
        // Remove \n from buffer
        buf[strcspn(buf, "\n")] = '\0';
        if (str_empty(buf)) {
            printf("empty\n");
            continue;
        }
        str_tolower(buf);
        directive d = line_to_directive(buf, labels, label_count);
        if (d.type != NOT_A_DIRECTIVE) {
            if (d.type == ORG) {
                if (org_program == 0xFFFF) { // If ORG has already been set
                    org_program = d.operand.value;
                } else {
                    printf("WARNING: ORG has already been set\n");
                }
            }
            else if (d.type == CONSTANT) {
                labels[label_count++] = d;
            }
        }
    }

    printf("Loaded %u labels\n", label_count);
    for (uint8_t i = 0; i < label_count; ++i) {
        printf("%s : %u\n", labels[i].label, labels[i].operand.value);
    }

    if (org_program == 0xFFFF) {
        printf("ERROR: ORG has not been set!\n");
        return 0;
    }

    rewind(f);

    uint16_t addr = org_program;
    cpu->pc = org_program;
    // second pass
    for (;;) {
        if (fgets(buf, 100, f) == NULL) {
            break;
        }
        // Remove \n from buffer
        buf[strcspn(buf, "\n")] = '\0';
        if (str_empty(buf)) {
            printf("empty\n");
            continue;
        }
        str_tolower(buf);
        if (is_directive(buf)) {
            printf("skipping directive %s\n", buf);
            continue;
        }
        printf("Read %s\n", buf);
        mnemonic m = line_to_mnemonic(buf, labels, label_count);
        addr += add_mnemonic_to_memory(cpu, &m, addr);
    }
    return 1;
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
    cpu c = (cpu) {0};

    printf("\nLoad Program\n");
    if (!load_program(&c, "f.asm")) {
        return 0;
    }
    printf("\nProgram loaded\n");
    print_cpu_state(&c);

    printf("\nExec program\n");
    exec_program(&c);
    printf("\nExecution ended\n");
    print_cpu_state(&c);
}

