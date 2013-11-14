#ifndef QSORT_ARG_H
#define QSORT_ARG_H

#include <sys/types.h>

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

/**
 * Multithread version of qsort_arg
 */
void qsort_arg_mt(void *a, size_t n, size_t es, int (*cmp)(const void *a, const void *b, void *arg), void *arg);

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif
