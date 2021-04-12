#ifndef TEST_H
#define TEST_H

char *current_test;
int test_n = 0;

#define test_fmt "TEST '%s' (%d) "
#define test_args current_test, test_n
#define TEST_FAILED test_fmt"\x1b[1;30;41mFAILED\x1b[m\n", test_args
#define TEST_SUCCESS test_fmt"\x1b[1;30;42mSUCCESS\x1b[m\n", test_args
#define CRIT_TEST_FAILED "[CRITICAL] "TEST_FAILED

#define TEST(name) {printf("\n");current_test = (name); test_n = 0;}

#define ASSERT_EQ(x,y) test_n++; if ((x) != (y)) { printf(TEST_FAILED); printf("    Expected: '%d'. Recieved: '%d'\n", x,y); } else { printf(TEST_SUCCESS);}

#define ASSERT_NEQ(x,y) test_n++; if ((x) == (y)) { printf(TEST_FAILED); printf("    Expected value different to '%d'\n", x); } else { printf(TEST_SUCCESS);}

#define ASSERT(x) test_n++; if (!(x)) { printf(TEST_FAILED); printf("    Expected: TRUE. Recieved: FALSE\n"); } else { printf(TEST_SUCCESS);}

#define CRIT_ASSERT_EQ(x,y) test_n++; if ((x) != (y)) { printf(CRIT_TEST_FAILED); printf("    Expected: '%d'. Recieved: '%d'\n", x,y); exit(0);} else { printf(TEST_SUCCESS);}

#define CRIT_ASSERT_NEQ(x,y) test_n++; if ((x) == (y)) { printf(CRIT_TEST_FAILED); printf("    Expected value different to '%d'\n", x); exit(0);} else { printf(TEST_SUCCESS);}

#define CRIT_ASSERT(x) test_n++; if (!(x)) { printf(CRIT_TEST_FAILED); printf("    Expected: TRUE. Recieved: FALSE\n");exit(0); } else { printf(TEST_SUCCESS);}

#endif
