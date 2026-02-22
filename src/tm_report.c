/*
 * Copyright (c) 2024 Microsoft Corporation
 * SPDX-License-Identifier: MIT
 */

/* Report helpers and tiny printf for the Thread-Metric test suite.
 *
 * Provides tm_printf() so test sources need no libc <stdio.h>.
 * Each porting layer supplies tm_putchar(int c) as the low-level
 * output bridge.
 */

#include <stdarg.h>
#include <stdlib.h>
#include "tm_api.h"

/* Print an unsigned long in decimal via tm_putchar(). */
static void tm_print_unsigned_long(unsigned long val)
{
    char buf[21];
    int i = 0;

    if (val == 0) {
        tm_putchar('0');
        return;
    }

    while (val > 0) {
        buf[i++] = (char) ('0' + (val % 10));
        val /= 10;
    }

    while (i > 0)
        tm_putchar(buf[--i]);
}

/* Print a signed int in decimal via tm_putchar().
 * Negation is done in unsigned to avoid UB on INT_MIN.
 */
static void tm_print_int(int val)
{
    if (val < 0) {
        tm_putchar('-');
        tm_print_unsigned_long((unsigned long) (-(unsigned) val));
    } else {
        tm_print_unsigned_long((unsigned long) val);
    }
}

/* Tiny printf: handles %d, %lu, %s, %%.  C89 / freestanding-safe. */
void tm_printf(const char *fmt, ...)
{
    va_list ap;
    const char *s;

    va_start(ap, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            tm_putchar(*fmt++);
            continue;
        }
        fmt++;
        switch (*fmt) {
        case 'l':
            fmt++;
            if (*fmt == 'u') {
                tm_print_unsigned_long(va_arg(ap, unsigned long));
            } else {
                /* Unknown %l_ combo -- print literal. */
                tm_putchar('%');
                tm_putchar('l');
                if (*fmt == '\0')
                    goto done;
                tm_putchar(*fmt);
            }
            break;
        case 'd':
            tm_print_int(va_arg(ap, int));
            break;
        case 's':
            s = va_arg(ap, const char *);
            if (s == NULL)
                s = "(null)";
            while (*s)
                tm_putchar(*s++);
            break;
        case '%':
            tm_putchar('%');
            break;
        case '\0':
            goto done;
        default:
            tm_putchar('%');
            tm_putchar(*fmt);
            break;
        }
        fmt++;
    }
done:
    va_end(ap);
}

void tm_report_finish(void)
{
    exit(0);
}

void tm_check_fail(const char *msg)
{
    tm_printf("%s", msg);
    exit(1);
}
