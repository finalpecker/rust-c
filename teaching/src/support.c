#define _CRT_SECURE_NO_WARNINGS

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "support.h"

void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    return ptr;
}

void *xrealloc(void *ptr, size_t size) {
    void *result = realloc(ptr, size);
    if (!result) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    return result;
}

char *str_dup_range(const char *start, size_t length) {
    char *text = (char *)xmalloc(length + 1);
    memcpy(text, start, length);
    text[length] = '\0';
    return text;
}

char *str_dup_c(const char *text) {
    return str_dup_range(text, strlen(text));
}

static void fatal_impl(const char *file, int line, const char *fmt, va_list ap) {
    fprintf(stderr, "error: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n  at %s:%d\n", file, line);
    exit(1);
}

void fatal_at(const char *file, int line, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fatal_impl(file, line, fmt, ap);
    va_end(ap);
}
