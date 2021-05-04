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
    char *dump_path;
} args;

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
    FILE *output_file;
    if (args->dump_path == NULL) {
        output_file = stdout;
    } else {
        output_file = fopen(args->dump_path, "w");
        if (!output_file) {
            fprintf(stderr, "Error while opening file %s\n", args->dump_path);
            exit(1);
        }
    }

    for (size_t i = 0; i < MAX_MEMORY; ++i) {
        // Only print newline when the "readable dump" argument has been given
        if (args->readable_dump && i % 16 == 0 && i != 0) {
            fprintf(output_file, "\n");
        }
        fprintf(output_file, FMT8" ", c->memory[i]);
    }
}

void handle_commands(cpu *cpu) {
    while (1) {
        char buf[20] = {0};
        printf("> ");
        if (fgets(buf, 20, stdin) == NULL) {
            exit(1);
        }
        // Removes new line
        buf[strcspn(buf, "\n")] = '\0';
        if (strcmp(buf, "ra") == 0) {
            printf("Register A: "FMT8"\n", cpu->a);
        } else if (strcmp(buf, "rb") == 0) {
            printf("Register B: "FMT8"\n", cpu->b);
        } else if (strcmp(buf, "rd") == 0) {
            printf("Register D: "FMT16"\n", cpu->d);
        } else if (str_prefix(buf, "next")) {
            const char *arg = buf + strlen("next");
            char *end;
            long l = strtol(arg, &end, 0);
            if (l == 0 && arg == end) {
                printf("Invalid argument\n");
                continue;
            }
            if (l > 0xFFFF) {
                printf("Argument is too big\n");
                continue;
            }
            uint16_t range = l & 0xFFFF;
            print_memory_range(cpu, cpu->pc, range);
        } else if (str_prefix(buf, "prev")) {
            const char *arg = buf + strlen("prev");
            char *end;
            long l = strtol(arg, &end, 0);
            if (l == 0 && arg == end) {
                printf("Invalid argument\n");
                continue;
            }
            if (l > 0xFFFF) {
                printf("Argument is too big\n");
                continue;
            }
            uint16_t range = l & 0xFFFF;
            if (cpu->pc - range < 0) range = cpu->pc; // Avoid going before 0x0000
            print_memory_range(cpu, cpu->pc - range, range);
        } else if (strcmp(buf, "status") == 0) {
            print_cpu_state(cpu);
        } else if (strcmp(buf, "pc") == 0) {
            printf("PC : "FMT8"\n", cpu->pc);
        } else if (strcmp(buf, "sp") == 0) {
            printf("SP : "FMT16"\n", cpu->sp);
        } else if (strcmp(buf, "labels") == 0) {
            printf("%d labels loaded\n", cpu->label_count);
            for (int i = 0; i < cpu->label_count; ++i) {
                printf("    %s: "FMT16"\n", cpu->labels[i].label, cpu->labels[i].operand.value);
            }
        } else if (strcmp(buf, "ports") == 0) {
            for (int i = 0; i < MAX_PORTS; ++i) {
                printf("    PORT%c: "FMT8"\n", 'a' + i, cpu->ports[i]);
            }
        } else {
            break;
        }
    }
}

void handle_args(args *args, int argc, char **argv) {
    // Skip the first argument which is the program name itself
    argc--;
    argv++;
    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--step") == 0 || strcmp(argv[i], "-s") == 0) {
            args->step = 1;
        }
        else if (strcmp(argv[i], "--dump") == 0 || strcmp(argv[i], "-d") == 0) {
            args->dump = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                args->dump_path = argv[i + 1];
                ++i;
            }
        }
        else if (strcmp(argv[i], "--readable") == 0 || strcmp(argv[i], "-r") == 0) {
            args->readable_dump = 1;
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("6611 assembly emulator (v1.0)\n"
                   "Usage: ./run [options...]\n"
                   "Where options are:\n"
                   "\t--dump     -d  Dumps whole program's memory when completelly loaded.\n"
                   "\t--readable -r  Dumps whole program's memory in a more human reable format when completelly loaded.\n"
                   "\t--step     -s  Execute the program instruction per instruction.\n");
            exit(0);
        } else {
            fprintf(stderr, "Unknow argument `%s`\n", argv[i]);
        }
    }

    if (args->readable_dump && !args->dump) {
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
    desotroy_cpu(c);
}
