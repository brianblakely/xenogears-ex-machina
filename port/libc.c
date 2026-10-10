/*
 * PsyQ libc: the string, memory and formatting calls the game makes. rand
 * keeps its seed at its original address (libc_rand_seed), so the game's
 * random stream and snapshots of it match the original's.
 */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/stdarg.h"

extern u32 libc_rand_seed;

int rand(void) {
    libc_rand_seed = libc_rand_seed * 0x41C64E6D + 0x3039;
    return (libc_rand_seed >> 16) & 0x7FFF;
}

void srand(unsigned int seed) {
    libc_rand_seed = seed;
}

int abs(int i) {
    return i >= 0 ? i : -i;
}

void *memcpy(void *dst, const void *src, int n) {
    u8 *d = dst;
    const u8 *s = src;

    while (n-- > 0) {
        *d++ = *s++;
    }
    return dst;
}

void *memmove(void *dst, void *src, int n) {
    u8 *d = dst;
    u8 *s = src;

    if (d < s) {
        while (n-- > 0) {
            *d++ = *s++;
        }
    } else {
        d += n;
        s += n;
        while (n-- > 0) {
            *--d = *--s;
        }
    }
    return dst;
}

void *memset(void *dst, int c, int n) {
    u8 *d = dst;

    while (n-- > 0) {
        *d++ = c;
    }
    return dst;
}

void *bzero(unsigned char *p, int n) {
    return memset(p, 0, n);
}

void *memchr(void *s, int c, int n) {
    u8 *p = s;

    for (; n > 0; n--, p++) {
        if (*p == (u8)c) {
            return p;
        }
    }
    return NULL;
}

int strlen(const char *s) {
    const char *p = s;

    while (*p != '\0') {
        p++;
    }
    return p - s;
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;

    while ((*d++ = *src++) != '\0') {
    }
    return dst;
}

char *strcat(char *dst, const char *src) {
    strcpy(dst + strlen(dst), src);
    return dst;
}

int strcmp(const char *a, const char *b) {
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *(const u8 *)a - *(const u8 *)b;
}

int toupper(int c) {
    return c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c;
}

int tolower(int c) {
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

/* Format an unsigned value in `base` into `out` (reversed), returning its length. */
static s32 xem_format_digits(char *out, u32 value, u32 base, s32 upper) {
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    s32 n = 0;

    do {
        out[n++] = digits[value % base];
        value /= base;
    } while (value != 0);
    return n;
}

/* The conversions the game uses (%d %u %x %X %o %c %s %p %%), with the flags
 * '-', '0', ' ', '+', a width, a precision and the 'l'/'h' size letters. */
int vsprintf(char *s, const char *format, va_list ap) {
    char *out = s;

    while (*format != '\0') {
        char digits[12];
        const char *text;
        s32 left = 0, zero = 0, plus = 0, space = 0;
        s32 width = 0, precision = -1, length, pad, sign = 0;
        char c;

        if (*format != '%') {
            *out++ = *format++;
            continue;
        }
        format++;
        for (;; format++) {
            if (*format == '-') left = 1;
            else if (*format == '0') zero = 1;
            else if (*format == '+') plus = 1;
            else if (*format == ' ') space = 1;
            else break;
        }
        if (*format == '*') {
            width = va_arg(ap, int);
            format++;
        }
        while (*format >= '0' && *format <= '9') {
            width = width * 10 + (*format++ - '0');
        }
        if (*format == '.') {
            precision = 0;
            format++;
            while (*format >= '0' && *format <= '9') {
                precision = precision * 10 + (*format++ - '0');
            }
        }
        while (*format == 'l' || *format == 'h') {
            format++;
        }
        c = *format++;
        switch (c) {
        case 'd':
        case 'i': {
            s32 value = va_arg(ap, int);
            u32 magnitude = value < 0 ? -(u32)value : (u32)value;

            sign = value < 0 ? '-' : plus ? '+' : space ? ' ' : 0;
            length = xem_format_digits(digits, magnitude, 10, 0);
            text = NULL;
            break;
        }
        case 'u':
            length = xem_format_digits(digits, va_arg(ap, unsigned int), 10, 0);
            text = NULL;
            break;
        case 'x':
        case 'X':
        case 'p':
            length = xem_format_digits(digits, va_arg(ap, unsigned int), 16, c == 'X');
            text = NULL;
            break;
        case 'o':
            length = xem_format_digits(digits, va_arg(ap, unsigned int), 8, 0);
            text = NULL;
            break;
        case 'c':
            digits[0] = (char)va_arg(ap, int);
            length = 1;
            text = NULL;
            break;
        case 's':
            text = va_arg(ap, char *);
            if (text == NULL) {
                text = "(null)";
            }
            length = strlen(text);
            if (precision >= 0 && length > precision) {
                length = precision;
            }
            break;
        case '%':
            *out++ = '%';
            continue;
        default:
            *out++ = c;
            continue;
        }
        if (text == NULL && precision > length) {
            /* Leading zeros up to the precision. */
            while (length < precision) {
                digits[length++] = '0';
            }
        }
        pad = width - length - (sign != 0);
        if (!left && !(zero && text == NULL)) {
            for (; pad > 0; pad--) *out++ = ' ';
        }
        if (sign) *out++ = sign;
        if (!left && zero && text == NULL) {
            for (; pad > 0; pad--) *out++ = '0';
        }
        if (text != NULL) {
            s32 i;
            for (i = 0; i < length; i++) *out++ = text[i];
        } else {
            while (length > 0) *out++ = digits[--length];
        }
        for (; pad > 0; pad--) *out++ = ' ';
    }
    *out = '\0';
    return out - s;
}

int sprintf(char *s, const char *format, ...) {
    va_list ap;
    int n;

    va_start(ap, format);
    n = vsprintf(s, format, ap);
    va_end(ap);
    return n;
}
