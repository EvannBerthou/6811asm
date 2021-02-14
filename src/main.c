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
        .names = {"staa", "sta"}, .name_count = 2,
        .codes = {[DIRECT]=0x97, [EXTENDED]=0xB7},
        .operands = { DIRECT, EXTENDED }
    },
    {
        .names = {"stab", "stb"}, .name_count = 2,
        .codes = {[DIRECT]=0xD7, [EXTENDED]=0xF7},
        .operands = { DIRECT, EXTENDED }
    },
    {
        .names = {"aba"}, .name_count = 1,
        .codes = {[INHERENT]=0x1B},
        .operands = { INHERENT }
    },
    {
        .names = {"bra"}, .name_count = 1,
        .codes = {[RELATIVE]=0x20},
        .operands = { RELATIVE }
    },
};
#define INSTRUCTION_COUNT ((uint8_t)(sizeof(instructions) / sizeof(instructions[0])))

const char *directives_name[] = { "org", "equ" };
#define DIRECTIVE_COUNT ((uint8_t)(sizeof(directives_name) / sizeof(directives_name[0])))

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

const char *file_name = "f.asm";
uint32_t file_line = 0;

#define ERROR(f_, ...) do {\
    printf("[ERROR] l.%u: "f_".\n", file_line, __VA_ARGS__); \
    exit(1); \
} while (0)

#define INFO(f_) printf("[INFO] "f_"\n")


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

void INST_STA_DIR(cpu *cpu) {
    uint8_t addr = cpu->memory[++cpu->pc]; // Get value of the next operand
    cpu->memory[addr] = cpu->a;
}

void INST_STA_EXT(cpu *cpu) {
    uint8_t addr = cpu->memory[++cpu->pc]; // Get value of the next operand
    cpu->memory[addr] = cpu->a;
}

void INST_STB_DIR(cpu *cpu) {
    uint8_t addr = cpu->memory[++cpu->pc]; // Get value of the next operand
    cpu->memory[addr] = cpu->b;
}

void INST_STB_EXT(cpu *cpu) {
    uint8_t addr = cpu->memory[++cpu->pc]; // Get value of the next operand
    cpu->memory[addr] = cpu->b;
}

void INST_BRA(cpu *cpu) {
    printf("Before jump "FMT8"\n", cpu->pc);
    uint8_t jmp = cpu->memory[++cpu->pc]; // Get value of the next operand
    cpu->pc += (int8_t) jmp;
    printf("After jump "FMT8"\n", cpu->pc);
}

/*****************************
*           Utils            *
*****************************/

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

void print_memory_range(cpu *cpu, uint16_t from, uint16_t len) {
    for (uint16_t i = 0; i < len; ++i) {
        printf("%04x: %02x\n", i, cpu->memory[from + i]);
    }
}

void print_cpu_state(cpu *cpu) {
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

/*****************************
*          Assembly          *
*****************************/

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
        return (operand) {directive->operand.value, directive->operand.type};
    }

    // Find where the number starts (skips non digit but keeps hex values)
    const char *number_part = str;
    while (!isdigit(*number_part) && (*number_part < 'a' || *number_part > 'f')) number_part++;
    // Convert to a number
    char *end;
    long l = strtol(number_part, &end, 16);
    if (l == 0 && number_part == end) {
        ERROR("%s %s", "is not a valid hex number", str);
    }
    uint16_t operand_value = l & 0xFFFF; // Keep the value below 0xFFFF
    operand_type type = get_operand_type(str, operand_value);
    if (type == NONE) {
        ERROR("%s `%s` %s", "The operand", str, "is neither a constant or a label");
    }
    return (operand) {operand_value, type};
}

mnemonic line_to_mnemonic(char *line, directive *labels, uint8_t label_count) {
    char *parts[3] = {0};
    uint8_t nb_parts = split_by_space(line, parts, 3);
    if (nb_parts > 2) {
        ERROR("`%s` %s", line, "has many operands");
    }

    instruction *inst = opcode_str_to_hex(parts[0]);
    if (inst == NULL) {
        ERROR("%s is an undefined (or not implemented) instruction", parts[0]);
    }

    uint8_t need_operand = inst->operands[0] != NONE && inst->operands[0] != INHERENT;
    if (need_operand != (nb_parts - 1)) { // need an operand but none were given
        ERROR("%s instruction requires %d operand but %d recieved\n", parts[0], need_operand, nb_parts - 1);
    }

    mnemonic result = {0};
    if (need_operand) {
        result.operand = get_operand_value(parts[1], labels, label_count);
        if (inst->operands[0] == RELATIVE) {
            result.operand.type = RELATIVE;
            if (result.operand.value > 0xFF) {
                ERROR("Relative addressing mode only supports 8 bits operands ("FMT16">0xFF)",
                      result.operand.value);
            }
            result.operand.value = result.operand.value & 0xFF;
        }
        // Checks if the given addressig mode is used by this instruction
        else if (!is_valid_operand_type(inst, result.operand.type)) {
            ERROR("%s does not support %s addressing mode\n", parts[0], "TODO");
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
    if (m->operand.type != NONE) {
        // Only extended uses 2 bytes for the operand
        // TODO: Certain instruction such as CPX uses 2 operands even for immediate mode
        if (m->operand.type == EXTENDED) {
            cpu->memory[addr + written++] = (m->operand.value >> 8) & 0xFF;
            cpu->memory[addr + written++] = m->operand.value & 0xFF;
        } else if (m->operand.type != NONE && m->operand.type != INHERENT) {
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
            ERROR("%s", "equ format : <LABEL> equ <VALUE>\n");
        }
        operand operand = get_operand_value(parts[2], labels, label_count);
        return (directive) {strdup(parts[0]), {operand.value, operand.type}, CONSTANT};
    }
    if (strstr(line, "org")) {
        char *parts[2] = {0};
        uint8_t nb_parts = split_by_space(line, parts, 2);
        if (nb_parts != 2) {
            ERROR("%s", "ORG format : ORG <ADDR> ($<VALUE>)\n");
        }

        operand operand = get_operand_value(parts[1], labels, label_count);
        if (operand.type == NONE) {
            ERROR("%s", "No operand found\n");
        }
        return (directive) {NULL, {operand.value, EXTENDED}, ORG};
    }

    return (directive) {NULL, {0, NONE}, NOT_A_DIRECTIVE};
}

int load_program(cpu *cpu, const char *file_path) {
    FILE *f = fopen(file_path, "r");
    if (!f) {
        printf("Error opennig file : %s\n", file_path);
        exit(1);
    }

    uint16_t org_program = 0x0;
    directive labels[MAX_LABELS] = {0};
    uint8_t label_count = 0;

    // first pass
    char buf[100];
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
        directive d = line_to_directive(buf, labels, label_count);
        if (d.type != NOT_A_DIRECTIVE) {
            if (d.type == ORG) {
                org_program = d.operand.value;
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
    printf("\n");

    if (org_program == 0xFFFF) {
        ERROR("%s", "ORG has not been set!\n");
    }

    INFO("First pass done with success");
    rewind(f);

    uint16_t addr = org_program;
    cpu->pc = org_program;
    // second pass
    for (;;) {
        if (fgets(buf, 100, f) == NULL) {
            break;
        }
        // Remove \n from buffer buf[strcspn(buf, "\n")] = '\0'; if (str_empty(buf)) {
        buf[strcspn(buf, "\n")] = '\0';
        if (str_empty(buf)) {
            continue;
        }
        str_tolower(buf);
        if (is_directive(buf)) {
            continue;
        }
        printf("Read %s\n", buf);
        mnemonic m = line_to_mnemonic(buf, labels, label_count);
        addr += add_mnemonic_to_memory(cpu, &m, addr);
    }
    return 1;
}

void (*instr_func[0x100]) (cpu *cpu) = {INST_NOP};

void exec_program(cpu *cpu) {
    while (cpu->memory[cpu->pc] != 0x00) {
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
    instr_func[0x97] = INST_STA_DIR;
    instr_func[0xB7] = INST_STA_EXT;
    instr_func[0xD7] = INST_STB_DIR;
    instr_func[0xF7] = INST_STB_EXT;
    instr_func[0x20] = INST_BRA;
    cpu c = (cpu) {0};

    INFO("Loading program");
    if (!load_program(&c, file_name)) {
        return 0;
    }
    INFO("Loading sucess");
    print_cpu_state(&c);

    INFO("Execution program");
    exec_program(&c);
    INFO("Execution ended");
    print_cpu_state(&c);
}

