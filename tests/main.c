#include "../src/emulator.h"

void test(int expect, int recieved, const char *test_name) {
    if (recieved != expect) {
        printf("TEST '%s' FAILED\n", test_name);
        printf("    Expected: %d. Recieved: %d\n", expect, recieved);
        exit(0);
    }
    printf("TEST '%s' SUCCESS\n", test_name);
}

int main() {
    add_instructions_func();
    cpu c = (cpu) {0};

    if (!load_program(&c, file_name)) {
        return 1;
    }
    test(0xC150, c.pc, "org_test1");
    test(0xC151, c.pc, "org_test2");
    return 0;
}
