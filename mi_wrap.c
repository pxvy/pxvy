#include <mimalloc.h>

void *__wrap_malloc(size_t n)           { return mi_malloc(n); }
void *__wrap_calloc(size_t n, size_t s) { return mi_calloc(n, s); }
void *__wrap_realloc(void *p, size_t n) { return mi_realloc(p, n); }
void  __wrap_free(void *p)              { mi_free(p); }