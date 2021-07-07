#define EMULATOR_IMPLEMENTATION
#include "emulator.h"

typedef struct {
    struct {
        uint8_t step          : 1;
        uint8_t dump          : 1;
        uint8_t from_dump     : 1;
        uint8_t readable_dump : 1;
        uint8_t print_info    : 1;
    };
} args;

typedef enum {
    NO_COMMAND = 0,
    REGISTER_A,
    REGISTER_B,
    REGISTER_D,
    NEXT,
    PREVIOUS,
    STATUS,
    PC,
    SP,
    LABELS,
    PORTS,
    CONTINUE,
    COMMAND_COUNT
} command_type;

typedef struct {
    const char *name;
    const char *sh_name; // Short name
    const command_type type;
} command;

const command commands[] = {
    {"registera", "ra", REGISTER_A},
    {"registerb", "rb", REGISTER_B},
    {"registerd", "rd", REGISTER_D},
    {"next", "n", NEXT},
    {"previous", "p", PREVIOUS},
    {"status", "s", STATUS},
    {"pc", NULL, PC},
    {"SP", NULL, SP},
    {"labels", "ls", LABELS},
    {"ports", "ps", PORTS},
    {"continue", "c", CONTINUE},
};

void print_memory_range(cpu *cpu, uint16_t from, uint16_t len) {
    for (uint16_t i = from; i < from + len; ++i) {
        printf("%04x: %02x\n", i, cpu->memory[i]);
        if (i + 1 == 0xFFFF) {
            printf("Outside of memory range\n");
            return;
        }
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

void dump_memory(const cpu *c, args *args) {
    FILE *output_file = stdout;
    for (size_t i = 0; i < MAX_MEMORY; ++i) {
        // Only print newline when the "readable dump" argument has been given
        if (args->readable_dump && i % 16 == 0 && i != 0) {
            fprintf(output_file, "\n");
        }
        fprintf(output_file, FMT8" ", c->memory[i]);
    }
    printf("\n");
}

static int cmp_name(const char *s1, const char *s2) {
    while (*s1 || *s2) {
        if (*s1 == ' ') return 1;
        if (*s1 != *s2) return 0;
        s1++;
        s2++;
    }
    return 1;
}

void handle_commands(cpu *cpu) {
    static command_type last_type = NO_COMMAND;
    static int last_arg = 0xFFFF;
    while (1) {
        char buf[20] = {0};
        printf("> ");
        if (fgets(buf, 20, stdin) == NULL) {
            exit(1);
        }

        // Removes new line
        buf[strcspn(buf, "\n")] = '\0';
        
        command_type cmd_type = NO_COMMAND;
        for (size_t i = 0; i < COMMAND_COUNT - 1; i++) {
            const command *cmd = &commands[i];
            if (cmp_name(buf, cmd->name)|| (cmd->sh_name != NULL && cmp_name(buf, cmd->sh_name))) {
                cmd_type = cmd->type;
                break;
            }
        }

        if (cmd_type == NO_COMMAND) {
            cmd_type = last_type;
        }

        last_type = cmd_type;
        
        switch (cmd_type) {
            case REGISTER_A: printf("Register A: "FMT8"\n", cpu->a); break;
            case REGISTER_B: printf("Register B: "FMT8"\n", cpu->b); break;
            case REGISTER_D: printf("Register D: "FMT16"\n", cpu->d); break;
            case NEXT: { 
                const char *arg = buf + strlen("next");
                char *end;
                long l = strtol(arg, &end, 0);
                if (l == 0 && arg == end) {
                    if (last_arg != 0xFFFF) {
                        l = last_arg;
                    } else {
                        printf("Invalid argument\n");
                        continue;
                    }
                }
                if (l > 0xFFFF) {
                    printf("Argument is too big\n");
                    continue;
                }
                uint16_t range = l & 0xFFFF;
                last_arg = range;
                print_memory_range(cpu, cpu->pc, range);
            } break;
            case PREVIOUS: {
                const char *arg = buf + strlen("prev");
                char *end;
                long l = strtol(arg, &end, 0);
                if (l == 0 && arg == end) {
                    if (last_arg != 0xFFFF) {
                        l = last_arg;
                    } else {
                        printf("Invalid argument\n");
                        continue;
                    }
                }
                if (l > 0xFFFF) {
                    printf("Argument is too big\n");
                    continue;
                }
                uint16_t range = l & 0xFFFF;
                if (cpu->pc - range < 0) range = cpu->pc; // Avoid going before 0x0000
                last_arg = range;
                print_memory_range(cpu, cpu->pc - range, range);
            } break;
            case STATUS: print_cpu_state(cpu); break;
            case PC: printf("PC : "FMT8"\n", cpu->pc); break;
            case SP: printf("SP : "FMT16"\n", cpu->sp); break;
            case LABELS: {
                printf("%d labels loaded\n", cpu->labels.count);
                for (int i = 0; i < cpu->labels.count; ++i) {
                    printf("\t%s: "FMT16"\n", cpu->label[i].label, cpu->label[i].operand.value);
                }
            } break;
            case PORTS: {
                for (int i = 0; i < MAX_PORTS; ++i) {
                    printf("\tPORT%c: "FMT8"\n", 'a' + i, cpu->ports[i]);
                }
            } break;
            case CONTINUE: return;
            default: break;
        }
    }
}

void print_help() {
    printf("6611 assembly emulator (v1.0)\n"
            "Usage: ./run [options...]\n"
            "Where options are:\n"
            "\t--dump     -d  Dumps whole program's memory when completelly loaded.\n"
            "\t--readable -r  Dumps whole program's memory in a more human reable format when completelly loaded.\n"
            "\t--step     -s  Execute the program instruction per instruction.\n");
    exit(0);
}

void handle_args(args *args, int argc, char **argv) {
    // Skip the first argument which is the program name itself
    argc--;
    argv++;
    for (int i = 0; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] != '-') {
            char *str = argv[i];
            str++; //Skip '-'
            while (*str) {
                switch (*str) {
                    case 's': args->step = 1; break;
                    case 'd': args->dump = 1; break;
                    case 'r': args->readable_dump = 1; break;
                    default: ERROR("Unknown argument `%c`", *str);
                }
                str++;
            }
        }

        if (strcmp(argv[i], "--step") == 0 || strcmp(argv[i], "-s") == 0) {
            args->step = 1;
        }
        else if (strcmp(argv[i], "--dump") == 0 || strcmp(argv[i], "-d") == 0) {
            args->dump = 1;
        }
        else if (strcmp(argv[i], "--readable") == 0 || strcmp(argv[i], "-r") == 0) {
            args->readable_dump = 1;
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help();
        } else {
            fprintf(stderr, "Unknow argument `%s`\n", argv[i]);
        }
    }

    if (args->readable_dump && !args->dump) {
        args->readable_dump = 0;
        args->dump = 0;
        INFO("%s", "--readable argument ignored as you need to use the --dump too.");
    }
}

void exec_program_step(cpu *cpu) {
    while (cpu->memory[cpu->pc] != 0x00) {
        printf("Next inst : "FMT8"\n", cpu->memory[cpu->pc]);
        handle_commands(cpu);

        uint8_t inst = cpu->memory[cpu->pc];
        if (instr_func[inst] != NULL) {
            (*instr_func[inst])(cpu); // Call the function with this opcode
        }
        cpu->pc++;
    }
    printf("Execution ended, you can still see last values\n");
    handle_commands(cpu);
}

int main(int argc, char **argv) {
    args args = {0};
    handle_args(&args, argc, argv);

    cpu *c = new_cpu("f.asm");

    if (args.dump) {
        dump_memory(c, &args);
    } else {
        if (args.step) {
            exec_program_step(c);
        } else {
            exec_program(c);
        }
    }
    destroy_cpu(c);
}
