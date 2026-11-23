#ifndef MINI_STDLIB_H
#define MINI_STDLIB_H
#include <stddef.h>
void *malloc(size_t n);
void *calloc(size_t n, size_t m);
void *realloc(void *p, size_t n);
void free(void *p);
int abs(int x);
long strtol(const char *s, char **ende, int basis);
int atoi(const char *s);
void qsort(void *b, size_t n, size_t g, int (*vgl)(const void *, const void *));
#define NULL_OK 1
#endif
