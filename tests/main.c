#include "../src/emulator.h"

char *current_test;
int test_n = 1;

#define test_fmt "TEST '%s' (%d) "
#define test_args current_test, test_n
#define TEST_FAILED test_fmt"FAILED\n", test_args
#define TEST_SUCCESS test_fmt"SUCCESS\n", test_args

#define TEST(name) {printf("\n");current_test = (name); test_n = 1;}

#define ASSERT_EQ(x,y) test_n++; if ((x) != (y)) { printf(TEST_FAILED); printf("    Expected: '%d'. Recieved: '%d'\n", x,y); } else { printf(TEST_SUCCESS);}

#define ASSERT_NEQ(x,y) test_n++; if ((x) == (y)) { printf(TEST_FAILED); printf("    Expected value different to '%d'\n", x); } else { printf(TEST_SUCCESS);}

#define ASSERT(x) test_n++; if (!(x)) { printf(TEST_FAILED); printf("    Expected: TRUE. Recieved: FALSE\n"); } else { printf(TEST_SUCCESS);}

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

    return 0;
}
