#include <stdbool.h>
#include <stdio.h>

#include "compiler.h"

static void print_i64_line(long long value) {
    char buffer[32];
    char *cursor = buffer + sizeof(buffer);
    unsigned long long magnitude;
    bool negative = value < 0;

    *--cursor = '\0';
    if (negative) {
        magnitude = (unsigned long long)(-(value + 1)) + 1ULL;
    } else {
        magnitude = (unsigned long long)value;
    }

    do {
        *--cursor = (char)('0' + (magnitude % 10ULL));
        magnitude /= 10ULL;
    } while (magnitude != 0ULL);

    if (negative) {
        *--cursor = '-';
    }

    fputs(cursor, stdout);
    fputc('\n', stdout);
}

static void usage(const char *argv0) {
    fprintf(stderr, "usage: %s <source.rx>\n", argv0);
}

int main(int argc, char **argv) {
    long long result = 0;

    if (argc != 2) {
        usage(argv[0]);
        return 1;
    }

    if (mini_rustc_run_file(argv[1], &result) != 0) {
        return 1;
    }

    print_i64_line(result);
    return 0;
}
