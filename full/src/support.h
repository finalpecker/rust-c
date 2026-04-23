#ifndef MINI_RUSTC_SUPPORT_H
#define MINI_RUSTC_SUPPORT_H

#include <stddef.h>

void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
char *str_dup_range(const char *start, size_t length);
char *str_dup_c(const char *text);
void fatal_at(const char *file, int line, const char *fmt, ...);

#define FATAL(...) fatal_at(__FILE__, __LINE__, __VA_ARGS__)

#endif
