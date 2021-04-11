#include "emulator.h"

int main(int argc, char **argv) {
    args args = {0};
    handle_args(&args, argc, argv);

    add_instructions_func();

    cpu c = (cpu) {0};
    set_default_ddr(&c);

    //INFO("%s", "Loading program");
    if (!load_program(&c, file_name)) {
        return 0;
    }

    if (args.dump) {
        dump_memory(&c, &args);
    } else {
        //INFO("%s", "Loading sucess");
        //INFO("%s", "Execution program");
        exec_program(&c, args.step);
        //INFO("%s", "Execution ended");
    }
}
