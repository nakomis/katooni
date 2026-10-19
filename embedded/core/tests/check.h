/* A deliberately tiny test harness: each test file is its own program, and fails if any CHECK does. */
#ifndef KATOONI_CHECK_H
#define KATOONI_CHECK_H

#include <stdio.h>
#include <string.h>

static int check_failures, check_count;

#define CHECK(cond)                                                                    \
    do {                                                                               \
        check_count++;                                                                 \
        if (!(cond)) {                                                                 \
            check_failures++;                                                          \
            fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond);   \
        }                                                                              \
    } while (0)

#define CHECK_INT(got, want)                                                                         \
    do {                                                                                             \
        long long g_ = (long long)(got), w_ = (long long)(want);                                     \
        check_count++;                                                                               \
        if (g_ != w_) {                                                                              \
            check_failures++;                                                                        \
            fprintf(stderr, "%s:%d: %s is %lld, want %lld\n", __FILE__, __LINE__, #got, g_, w_);     \
        }                                                                                            \
    } while (0)

#define CHECK_STR(got, want)                                                                           \
    do {                                                                                               \
        const char *g_ = (got), *w_ = (want);                                                          \
        check_count++;                                                                                 \
        if (strcmp(g_, w_) != 0) {                                                                     \
            check_failures++;                                                                          \
            fprintf(stderr, "%s:%d: %s is \"%s\", want \"%s\"\n", __FILE__, __LINE__, #got, g_, w_);   \
        }                                                                                              \
    } while (0)

#define CHECK_HAS(haystack, needle)                                                                     \
    do {                                                                                                \
        const char *h_ = (haystack), *n_ = (needle);                                                    \
        check_count++;                                                                                  \
        if (!strstr(h_, n_)) {                                                                          \
            check_failures++;                                                                           \
            fprintf(stderr, "%s:%d: \"%s\" doesn't contain \"%s\"\n", __FILE__, __LINE__, h_, n_);      \
        }                                                                                               \
    } while (0)

#define CHECK_DONE()                                                                    \
    do {                                                                                \
        printf("%d checks, %d failed\n", check_count, check_failures);                  \
        return check_failures ? 1 : 0;                                                  \
    } while (0)

#endif
