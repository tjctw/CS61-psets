// vim: set tabstop=8: -*- tab-width: 8 -*-
#include "lib.h"
#include "x86.h"

// lib.c
//
//    Functions useful in both kernel and applications.


// memcpy, memmove, memset, strcmp, strlen, strnlen
//    We must provide our own implementations.

void *memcpy(void *dst, const void *src, size_t n) {
    const char *s = (const char *) src;
    for (char *d = (char *) dst; n > 0; --n, ++s, ++d)
	*d = *s;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    const char *s = (const char *) src;
    char *d = (char *) dst;
    if (s < d && s + n > d) {
	s += n, d += n;
	while (n-- > 0)
	    *--d = *--s;
    } else
	while (n-- > 0)
	    *d++ = *s++;
    return dst;
}

void *memset(void *v, int c, size_t n) {
    for (char *p = (char *) v; n > 0; ++p, --n)
	*p = c;
    return v;
}

size_t strlen(const char *s) {
    size_t n;
    for (n = 0; *s != '\0'; ++s)
	++n;
    return n;
}

size_t strnlen(const char *s, size_t maxlen) {
    size_t n;
    for (n = 0; n != maxlen && *s != '\0'; ++s)
	++n;
    return n;
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    do {
	*d++ = *src++;
    } while (d[-1]);
    return dst;
}

int strcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b)
	++a, ++b;
    return ((unsigned char) *a > (unsigned char) *b)
	- ((unsigned char) *a < (unsigned char) *b);
}

char *strchr(const char *s, int c) {
    while (*s && *s != (char) c)
	++s;
    if (*s == (char) c)
	return (char *) s;
    else
	return NULL;
}


// rand, srand

static int rand_seed_set;
static unsigned rand_seed;

int rand(void) {
    if (!rand_seed_set)
	srand(819234718U);
    rand_seed = rand_seed * 1664525U + 1013904223U;
    return rand_seed & RAND_MAX;
}

void srand(unsigned seed) {
    rand_seed = seed;
    rand_seed_set = 1;
}


// console_vprintf, console_printf
//    Print a message onto the console, starting at the given cursor position.

// snprintf, vsnprintf
//    Format a string into a buffer.

static char *fill_numbuf(char *numbuf_end, uint32_t val, int base) {
    static const char upper_digits[] = "0123456789ABCDEF";
    static const char lower_digits[] = "0123456789abcdef";

    const char *digits = upper_digits;
    if (base < 0) {
	digits = lower_digits;
	base = -base;
    }

    *--numbuf_end = '\0';
    do {
	*--numbuf_end = digits[val % base];
	val /= base;
    } while (val != 0);
    return numbuf_end;
}

#define FLAG_ALT		(1<<0)
#define FLAG_ZERO		(1<<1)
#define FLAG_LEFTJUSTIFY	(1<<2)
#define FLAG_SPACEPOSITIVE	(1<<3)
#define FLAG_PLUSPOSITIVE	(1<<4)
static const char flag_chars[] = "#0- +";

void printer_vprintf(struct printer *p, int color, const char *format,
		     va_list val) {
#define NUMBUFSIZ 24
    char numbuf[NUMBUFSIZ];

    for (; *format; ++format) {
	if (*format != '%') {
	    p->putc(p, *format, color);
	    continue;
	}

	// process flags
	int flags = 0;
	for (++format; *format; ++format) {
	    const char *flagc = strchr(flag_chars, *format);
	    if (flagc)
		flags |= 1 << (flagc - flag_chars);
	    else
		break;
	}

	// process width
	int width = -1;
	if (*format >= '1' && *format <= '9') {
	    for (width = 0; *format >= '0' && *format <= '9'; )
		width = 10 * width + *format++ - '0';
	} else if (*format == '*') {
	    width = va_arg(val, int);
	    ++format;
	}

	// process precision
	int precision = -1;
	if (*format == '.') {
	    ++format;
	    if (*format >= '0' && *format <= '9') {
		for (precision = 0; *format >= '0' && *format <= '9'; )
		    precision = 10 * precision + *format++ - '0';
	    } else if (*format == '*') {
		precision = va_arg(val, int);
		++format;
	    }
	    if (precision < 0)
		precision = 0;
	}

	// process main conversion character
	int negative = 0;
	int numeric = 0;
	int base = 10;
	char *data;
	switch (*format) {
	case 'd': {
	    int x = va_arg(val, int);
	    data = fill_numbuf(numbuf + NUMBUFSIZ, x > 0 ? x : -x, 10);
	    if (x < 0)
		negative = 1;
	    numeric = 1;
	    break;
	}
	case 'u':
	print_unsigned: {
	    unsigned x = va_arg(val, unsigned);
	    data = fill_numbuf(numbuf + NUMBUFSIZ, x, base);
	    numeric = 1;
	    break;
	}
	case 'x':
	    base = -16;
	    goto print_unsigned;
	case 'X':
	    base = 16;
	    goto print_unsigned;
	case 'p': {
	    void *x = va_arg(val, void *);
	    data = fill_numbuf(numbuf + NUMBUFSIZ, (uintptr_t) x, -16);
	    data[-1] = 'x';
	    data[-2] = '0';
	    data -= 2;
	    break;
	}
	case 's':
	    data = va_arg(val, char *);
	    break;
	case 'C':
	    color = va_arg(val, int);
	    goto done;
	case 'c':
	    data = numbuf;
	    numbuf[0] = va_arg(val, int);
	    numbuf[1] = '\0';
	    break;
	normal:
	default:
	    data = numbuf;
	    numbuf[0] = (*format ? *format : '%');
	    numbuf[1] = '\0';
	    if (!*format)
		format--;
	    break;
	}

	int len;
	if (precision >= 0 && !numeric)
	    len = strnlen(data, precision);
	else
	    len = strlen(data);
	if (numeric && negative)
	    negative = '-';
	else if (flags & FLAG_PLUSPOSITIVE)
	    negative = '+';
	else if (flags & FLAG_SPACEPOSITIVE)
	    negative = ' ';
	else
	    negative = 0;
	int zeros;
	if (numeric && precision >= 0)
	    zeros = precision > len ? precision - len : 0;
	else if ((flags & (FLAG_ZERO | FLAG_LEFTJUSTIFY)) == FLAG_ZERO
		 && numeric && len + !!negative < width)
	    zeros = width - len - !!negative;
	else
	    zeros = 0;
	width -= len + zeros + !!negative;
	for (; !(flags & FLAG_LEFTJUSTIFY) && width > 0; --width)
	    p->putc(p, ' ', color);
	if (negative)
	    p->putc(p, negative, color);
	for (; zeros > 0; --zeros)
	    p->putc(p, '0', color);
	for (; len > 0; ++data, --len)
	    p->putc(p, *data, color);
	for (; width > 0; --width)
	    p->putc(p, ' ', color);
    done: ;
    }
}


struct console_printer {
    struct printer p;
    uint16_t *cursor;
};

static void console_putc(struct printer *p, unsigned char c, int color) {
    struct console_printer *cp = (struct console_printer *) p;
    if (cp->cursor >= console + CONSOLE_ROWS * CONSOLE_COLUMNS)
	cp->cursor = console;
    if (c == '\n') {
	int pos = (cp->cursor - console) % 80;
	for (; pos != 80; pos++)
	    *cp->cursor++ = ' ' | color;
    } else
	*cp->cursor++ = c | color;
}

int console_vprintf(int cpos, int color, const char *format, va_list val) {
    struct console_printer cp;
    cp.p.putc = console_putc;
    if (cpos < 0 || cpos >= CONSOLE_ROWS * CONSOLE_COLUMNS)
	cpos = 0;
    cp.cursor = console + cpos;
    printer_vprintf(&cp.p, color, format, val);
    return cp.cursor - console;
}

int console_printf(int cpos, int color, const char *format, ...) {
    va_list val;
    va_start(val, format);
    cpos = console_vprintf(cpos, color, format, val);
    va_end(val);
    return cpos;
}


struct string_printer {
    struct printer p;
    char *s;
    char *end;
};

static void string_putc(struct printer *p, unsigned char c, int color) {
    struct string_printer *sp = (struct string_printer *) p;
    if (sp->s < sp->end)
	*sp->s++ = c;
    (void) color;
}

int vsnprintf(char *s, size_t size, const char *format, va_list val) {
    struct string_printer sp;
    sp.p.putc = string_putc;
    sp.s = s;
    if (size) {
	sp.end = s + size - 1;
	printer_vprintf(&sp.p, 0, format, val);
	*sp.s = 0;
    }
    return sp.s - s;
}

int snprintf(char *s, size_t size, const char *format, ...) {
    va_list val;
    va_start(val, format);
    int n = vsnprintf(s, size, format, val);
    va_end(val);
    return n;
}


// console_clear
//    Erases the console and moves the cursor to the upper left (CPOS(0, 0)).

void console_clear(void) {
    for (int i = 0; i < CONSOLE_ROWS * CONSOLE_COLUMNS; ++i)
	console[i] = ' ' | 0x0700;
    cursorpos = 0;
}
