#include "../src/emulator.h"
#include "tests.h"

void init(cpu *c) {
    add_instructions_func();
    if (!load_program(c, file_name)) {
        return;
    }
}

int main() {
    cpu c = (cpu) {0};
    init(&c);

    TEST ("org_test1") {
        ASSERT_EQ(0xC150, c.pc)
        ASSERT(1)
        ASSERT(0)
    }

    TEST ("org_test2") {
        ASSERT_EQ(0xC150, c.pc)
        ASSERT_EQ(0xC151, c.pc)
        ASSERT_NEQ(2, 2)
    }

    TEST ("test") {
        CRIT_ASSERT(0);
        // Never reached
        CRIT_ASSERT(1);
    }

    return 0;
}
