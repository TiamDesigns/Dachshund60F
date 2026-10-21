/*
 * Minimal test harness for the host unit tests. Header-only, no dependencies.
 * Each test file includes this once, defines test functions, and returns
 * KB_TEST_RESULT() from main().
 */
#ifndef KB_TEST_H
#define KB_TEST_H

#include <stdio.h>

static int kb_test_failures;

#define KB_CHECK(cond)                                                       \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, \
                    #cond);                                                  \
            kb_test_failures++;                                              \
        }                                                                    \
    } while (0)

#define KB_CHECK_EQ(actual, expected)                                        \
    do {                                                                     \
        long long kb_a_ = (long long)(actual);                               \
        long long kb_e_ = (long long)(expected);                             \
        if (kb_a_ != kb_e_) {                                                \
            fprintf(stderr, "%s:%d: %s == %lld, expected %lld\n", __FILE__,  \
                    __LINE__, #actual, kb_a_, kb_e_);                        \
            kb_test_failures++;                                              \
        }                                                                    \
    } while (0)

#define KB_RUN(fn)                     \
    do {                               \
        printf("  %s\n", #fn);         \
        fn();                          \
    } while (0)

#define KB_TEST_RESULT() (kb_test_failures == 0 ? 0 : 1)

#endif /* KB_TEST_H */
