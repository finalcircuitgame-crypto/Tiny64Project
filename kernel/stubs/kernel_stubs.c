// Minimal libc/posix stubs for Tiny64 kernel build
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// --- sscanf stub ---
int __isoc99_sscanf(const char *str, const char *fmt, ...) {
    // Only supports %d and %x for simple integer parsing
    va_list ap;
    va_start(ap, fmt);
    int *out = va_arg(ap, int*);
    int n = 0;
    if (fmt[0] == ' ' && fmt[1] == '%' && (fmt[2] == 'd' || fmt[2] == 'x' || fmt[2] == 'X')) {
        int base = (fmt[2] == 'd') ? 10 : 16;
        *out = (int)strtol(str, NULL, base);
        n = 1;
    }
    va_end(ap);
    return n;
}

// --- String and memory ---
int strcasecmp(const char *a, const char *b) {
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
        if (ca != cb) return (unsigned char)ca - (unsigned char)cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncasecmp(const char *a, const char *b, size_t n) {
    while (n-- && *a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
        if (ca != cb) return (unsigned char)ca - (unsigned char)cb;
        a++; b++;
    }
    if (n == (size_t)-1) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

char *strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *d = (char *)malloc(len);
    if (d) memcpy(d, s, len);
    return d;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    return (char *)(last ? last : NULL);
}

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    if (d < s) {
        for (size_t i = 0; i < n; i++) d[i] = s[i];
    } else if (d > s) {
        for (size_t i = n; i > 0; i--) d[i-1] = s[i-1];
    }
    return dest;
}

char *strstr(const char *haystack, const char *needle) {
    size_t nlen = strlen(needle);
    if (!nlen) return (char *)haystack;
    for (; *haystack; haystack++) {
        if (!strncmp(haystack, needle, nlen)) return (char *)haystack;
    }
    return NULL;
}

// --- Character ---
int tolower(int c) { if (c >= 'A' && c <= 'Z') return c + 32; return c; }
int toupper(int c) { if (c >= 'a' && c <= 'z') return c - 32; return c; }

// --- Math ---
double fabs(double x) { return x < 0 ? -x : x; }

// --- Memory allocation ---
void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *p = malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

// --- File ops ---

// --- Directory ---
int mkdir(const char *path, ...) { return -1; }

// --- I/O stubs ---
void *stdout = 0;
void *stderr = 0;

// Helper functions for vfprintf
static void int_to_str(long value, char* str, int base) {
    static const char digits[] = "0123456789abcdef";
    int i = 0;
    int is_negative = 0;

    if (value < 0 && base == 10) {
        is_negative = 1;
        value = -value;
    }

    if (value == 0) {
        str[i++] = '0';
    } else {
        while (value > 0) {
            str[i++] = digits[value % base];
            value /= base;
        }
    }

    if (is_negative) {
        str[i++] = '-';
    }

    str[i] = '\0';

    // Reverse the string
    int len = i;
    for (int j = 0; j < len / 2; j++) {
        char temp = str[j];
        str[j] = str[len - 1 - j];
        str[len - 1 - j] = temp;
    }
}

static void uint_to_str(unsigned long value, char* str, int base) {
    static const char digits[] = "0123456789abcdef";
    int i = 0;

    if (value == 0) {
        str[i++] = '0';
    } else {
        while (value > 0) {
            str[i++] = digits[value % base];
            value /= base;
        }
    }

    str[i] = '\0';

    // Reverse the string
    int len = i;
    for (int j = 0; j < len / 2; j++) {
        char temp = str[j];
        str[j] = str[len - 1 - j];
        str[len - 1 - j] = temp;
    }
}

int vfprintf(void *stream, const char *fmt, va_list ap) {
    extern void serial_write_string(const char* str);
    int count = 0;

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 's': {
                    char* str = va_arg(ap, char*);
                    if (str) {
                        serial_write_string(str);
                        count += strlen(str);
                    } else {
                        serial_write_string("(null)");
                        count += 6;
                    }
                    break;
                }
                case 'd':
                case 'i': {
                    int value = va_arg(ap, int);
                    char buffer[32];
                    int_to_str(value, buffer, 10);
                    serial_write_string(buffer);
                    count += strlen(buffer);
                    break;
                }
                case 'u': {
                    unsigned int value = va_arg(ap, unsigned int);
                    char buffer[32];
                    uint_to_str(value, buffer, 10);
                    serial_write_string(buffer);
                    count += strlen(buffer);
                    break;
                }
                case 'x': {
                    unsigned int value = va_arg(ap, unsigned int);
                    char buffer[32];
                    uint_to_str(value, buffer, 16);
                    serial_write_string(buffer);
                    count += strlen(buffer);
                    break;
                }
                case 'p': {
                    void* ptr = va_arg(ap, void*);
                    char buffer[32];
                    if (ptr == NULL) {
                        serial_write_string("(nil)");
                        count += 5;
                    } else {
                        uint_to_str((unsigned long)(uintptr_t)ptr, buffer, 16);
                        serial_write_string("0x");
                        serial_write_string(buffer);
                        count += strlen(buffer) + 2;
                    }
                    break;
                }
                case 'c': {
                    int value = va_arg(ap, int);
                    char buf[2] = {value, 0};
                    serial_write_string(buf);
                    count++;
                    break;
                }
                case '%': {
                    serial_write_string("%");
                    count++;
                    break;
                }
                default: {
                    char buf[3] = {'%', *fmt, 0};
                    serial_write_string(buf);
                    count += 2;
                    break;
                }
            }
            fmt++;
        } else {
            char buf[2] = {*fmt, 0};
            serial_write_string(buf);
            count++;
            fmt++;
        }
    }

    return count;
}

int vsnprintf(char *str, size_t size, const char *fmt, va_list ap) {
    int count = 0;
    
    if (size == 0) return 0;
    
    while (*fmt && count < (int)size - 1) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 's': {
                    char* s = va_arg(ap, char*);
                    if (s) {
                        while (*s && count < (int)size - 1) {
                            *str++ = *s++;
                            count++;
                        }
                    }
                    break;
                }
                case 'd':
                case 'i': {
                    int val = va_arg(ap, int);
                    char buf[32];
                    int_to_str(val, buf, 10);
                    char* p = buf;
                    while (*p && count < (int)size - 1) {
                        *str++ = *p++;
                        count++;
                    }
                    break;
                }
                case 'u': {
                    unsigned int val = va_arg(ap, unsigned int);
                    char buf[32];
                    uint_to_str(val, buf, 10);
                    char* p = buf;
                    while (*p && count < (int)size - 1) {
                        *str++ = *p++;
                        count++;
                    }
                    break;
                }
                case 'x': {
                    unsigned int val = va_arg(ap, unsigned int);
                    char buf[32];
                    uint_to_str(val, buf, 16);
                    char* p = buf;
                    while (*p && count < (int)size - 1) {
                        *str++ = *p++;
                        count++;
                    }
                    break;
                }
                case 'c': {
                    char c = va_arg(ap, int);
                    if (count < (int)size - 1) {
                        *str++ = c;
                        count++;
                    }
                    break;
                }
                case '%': {
                    if (count < (int)size - 1) {
                        *str++ = '%';
                        count++;
                    }
                    break;
                }
                default:
                    // Unknown format specifier, just copy it
                    if (count < (int)size - 2) {
                        *str++ = '%';
                        *str++ = *fmt;
                        count += 2;
                    }
                    break;
            }
            fmt++;
        } else {
            *str++ = *fmt++;
            count++;
        }
    }
    
    *str = '\0';
    return count;
}

// --- strtol stub ---
long int strtol(const char *nptr, char **endptr, int base) {
    long int result = 0;
    int sign = 1;
    const char *s = nptr;

    // Skip whitespace
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' || *s == '\v') s++;

    // Handle sign
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    // Determine base if not specified
    if (base == 0) {
        if (*s == '0') {
            if (*(s+1) == 'x' || *(s+1) == 'X') {
                base = 16;
                s += 2;
            } else {
                base = 8;
                s++;
            }
        } else {
            base = 10;
        }
    } else if (base == 16 && *s == '0' && (*(s+1) == 'x' || *(s+1) == 'X')) {
        s += 2;
    }

    // Convert digits
    while (*s) {
        int digit;
        if (*s >= '0' && *s <= '9') {
            digit = *s - '0';
        } else if (*s >= 'a' && *s <= 'f') {
            digit = *s - 'a' + 10;
        } else if (*s >= 'A' && *s <= 'F') {
            digit = *s - 'A' + 10;
        } else {
            break;
        }

        if (digit >= base) break;

        result = result * base + digit;
        s++;
    }

    if (endptr) *endptr = (char *)s;
    return result * sign;
}

// --- atof stub (integer-based to avoid SSE) ---
double atof(const char *s) {
    // Use integer arithmetic to build the value, then convert to double at the end
    long long int_part = 0, frac_part = 0, frac_div = 1;
    int sign = 1;
    int in_fraction = 0;

    if (*s == '-') { sign = -1; s++; }

    while (*s) {
        if (*s >= '0' && *s <= '9') {
            if (!in_fraction) {
                int_part = int_part * 10 + (*s - '0');
            } else {
                frac_part = frac_part * 10 + (*s - '0');
                frac_div *= 10;
            }
        } else if (*s == '.') {
            in_fraction = 1;
        } else {
            break;
        }
        s++;
    }

    // Combine parts: int_part + frac_part/frac_div
    // To avoid floating point until the very end
    double result = (double)int_part + (double)frac_part / (double)frac_div;
    return result * sign;
}

// --- errno/ctype/system stubs ---
int *__errno_location(void) { static int e = 0; return &e; }
const unsigned short **__ctype_b_loc(void) { static const unsigned short c[384] = {0}; static const unsigned short *p = c + 128; return &p; }
int system(const char *cmd) { return -1; }
