#ifndef MINI_STDIO_H
#define MINI_STDIO_H
#include <stddef.h>
#include <stdarg.h>
int vsnprintf(char *p, size_t n, const char *fmt, va_list a);
int snprintf(char *p, size_t n, const char *fmt, ...);
#endif
