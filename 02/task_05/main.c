#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INT_VALUE ((int)(~0u >> 1))
#define SPEC_SIZE 48

typedef enum {
    STATUS_OK,
    STATUS_NULL_ARGUMENT,
    STATUS_NO_MEMORY,
    STATUS_INVALID_FORMAT,
    STATUS_INVALID_VALUE
} status_t;

typedef enum {
    FLAG_NONE,
    FLAG_RO,
    FLAG_ZR,
    FLAG_CV_LOWER,
    FLAG_CV_UPPER,
    FLAG_TO_LOWER,
    FLAG_TO_UPPER,
    FLAG_MI,
    FLAG_MU,
    FLAG_MD,
    FLAG_MF
} flag_t;

typedef enum {
    LEN_NONE,
    LEN_HH,
    LEN_H,
    LEN_L,
    LEN_LL,
    LEN_J,
    LEN_Z,
    LEN_T,
    LEN_BIG_L
} length_t;

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} text_buffer_t;

static status_t buffer_reserve(text_buffer_t *buffer, size_t extra)
{
    size_t needed = buffer->length + extra + 1;
    if (needed <= buffer->capacity) {
        return STATUS_OK;
    }
    size_t new_capacity = buffer->capacity == 0 ? 64 : buffer->capacity;
    while (new_capacity < needed) {
        new_capacity *= 2;
    }
    char *tmp = realloc(buffer->data, new_capacity);
    if (tmp == NULL) {
        return STATUS_NO_MEMORY;
    }
    buffer->data = tmp;
    buffer->capacity = new_capacity;
    return STATUS_OK;
}

static status_t buffer_append(text_buffer_t *buffer, const char *text, size_t length)
{
    status_t status = buffer_reserve(buffer, length);
    if (status != STATUS_OK) {
        return status;
    }
    memcpy(buffer->data + buffer->length, text, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
    return STATUS_OK;
}

static status_t buffer_append_char(text_buffer_t *buffer, char c)
{
    return buffer_append(buffer, &c, 1);
}

static status_t append_formatted(text_buffer_t *buffer, const char *spec, ...)
{
    va_list args;
    va_list copy;
    va_start(args, spec);
    va_copy(copy, args);
    int needed = vsnprintf(NULL, 0, spec, copy);
    va_end(copy);
    status_t status = STATUS_INVALID_FORMAT;
    if (needed >= 0) {
        status = buffer_reserve(buffer, (size_t)needed);
    }
    if (needed >= 0 && status == STATUS_OK) {
        vsnprintf(buffer->data + buffer->length, (size_t)needed + 1, spec, args);
        buffer->length += (size_t)needed;
    }
    va_end(args);
    return status;
}

static status_t append_roman(text_buffer_t *buffer, int value)
{
    const int values[] = {1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1};
    const char *symbols[] = {"M", "CM", "D", "CD", "C", "XC", "L", "XL", "X", "IX", "V", "IV", "I"};
    if (value < 1 || value > 3999) {
        return STATUS_INVALID_VALUE;
    }
    for (size_t i = 0; i < 13; i++) {
        while (value >= values[i]) {
            status_t status = buffer_append(buffer, symbols[i], strlen(symbols[i]));
            if (status != STATUS_OK) {
                return status;
            }
            value -= values[i];
        }
    }
    return STATUS_OK;
}

static status_t append_zeckendorf(text_buffer_t *buffer, unsigned int value)
{
    unsigned long long fib[48];
    char digits[48] = {0};
    size_t count = 2;
    size_t top = 0;
    if (value == 0) {
        return STATUS_INVALID_VALUE;
    }
    fib[0] = 1;
    fib[1] = 2;
    while (fib[count - 1] <= value) {
        fib[count] = fib[count - 1] + fib[count - 2];
        count++;
    }
    unsigned long long rest = value;
    int top_found = 0;
    for (size_t i = count; i > 0; i--) {
        if (fib[i - 1] <= rest) {
            digits[i - 1] = 1;
            rest -= fib[i - 1];
            if (!top_found) {
                top = i - 1;
                top_found = 1;
            }
        }
    }
    for (size_t i = 0; i <= top; i++) {
        status_t status = buffer_append_char(buffer, (char)('0' + digits[i]));
        if (status != STATUS_OK) {
            return status;
        }
    }
    return buffer_append_char(buffer, '1');
}

static status_t append_in_base(text_buffer_t *buffer, int value, int base, int uppercase)
{
    char digits[40];
    size_t count = 0;
    unsigned int magnitude = value < 0 ? 0u - (unsigned int)value : (unsigned int)value;
    if (base < 2 || base > 36) {
        base = 10;
    }
    do {
        unsigned int digit = magnitude % (unsigned int)base;
        digits[count++] = (char)(digit < 10 ? '0' + digit : (uppercase ? 'A' : 'a') + digit - 10);
        magnitude /= (unsigned int)base;
    } while (magnitude != 0);
    if (value < 0) {
        status_t status = buffer_append_char(buffer, '-');
        if (status != STATUS_OK) {
            return status;
        }
    }
    for (size_t i = count; i > 0; i--) {
        status_t status = buffer_append_char(buffer, digits[i - 1]);
        if (status != STATUS_OK) {
            return status;
        }
    }
    return STATUS_OK;
}

static int digit_value(char c, int uppercase)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (!uppercase && c >= 'a' && c <= 'z') {
        return c - 'a' + 10;
    }
    if (uppercase && c >= 'A' && c <= 'Z') {
        return c - 'A' + 10;
    }
    return -1;
}

static status_t parse_in_base(const char *text, int base, int uppercase, long long *value)
{
    const unsigned long long limit = ~0ULL >> 1;
    int negative = 0;
    unsigned long long result = 0;
    if (text == NULL) {
        return STATUS_NULL_ARGUMENT;
    }
    if (base < 2 || base > 36) {
        base = 10;
    }
    if (*text == '-') {
        negative = 1;
        text++;
    }
    if (*text == '\0') {
        return STATUS_INVALID_VALUE;
    }
    for (; *text != '\0'; text++) {
        int digit = digit_value(*text, uppercase);
        if (digit < 0 || digit >= base) {
            return STATUS_INVALID_VALUE;
        }
        if (result > (limit - (unsigned long long)digit) / (unsigned long long)base) {
            return STATUS_INVALID_VALUE;
        }
        result = result * (unsigned long long)base + (unsigned long long)digit;
    }
    *value = negative ? -(long long)result : (long long)result;
    return STATUS_OK;
}

static status_t append_decimal_from_base(text_buffer_t *buffer, const char *text, int base, int uppercase)
{
    long long value = 0;
    status_t status = parse_in_base(text, base, uppercase, &value);
    if (status != STATUS_OK) {
        return status;
    }
    return append_formatted(buffer, "%lld", value);
}

static status_t append_dump(text_buffer_t *buffer, const void *memory, size_t size)
{
    const unsigned char *bytes = memory;
    for (size_t i = 0; i < size; i++) {
        if (i > 0) {
            status_t status = buffer_append_char(buffer, ' ');
            if (status != STATUS_OK) {
                return status;
            }
        }
        for (int bit = 7; bit >= 0; bit--) {
            status_t status = buffer_append_char(buffer, (char)('0' + ((bytes[i] >> bit) & 1)));
            if (status != STATUS_OK) {
                return status;
            }
        }
    }
    return STATUS_OK;
}

static flag_t detect_flag(const char *p)
{
    if (p[0] == 'R' && p[1] == 'o') {
        return FLAG_RO;
    }
    if (p[0] == 'Z' && p[1] == 'r') {
        return FLAG_ZR;
    }
    if (p[0] == 'C' && p[1] == 'v') {
        return FLAG_CV_LOWER;
    }
    if (p[0] == 'C' && p[1] == 'V') {
        return FLAG_CV_UPPER;
    }
    if (p[0] == 't' && p[1] == 'o') {
        return FLAG_TO_LOWER;
    }
    if (p[0] == 'T' && p[1] == 'O') {
        return FLAG_TO_UPPER;
    }
    if (p[0] == 'm' && p[1] == 'i') {
        return FLAG_MI;
    }
    if (p[0] == 'm' && p[1] == 'u') {
        return FLAG_MU;
    }
    if (p[0] == 'm' && p[1] == 'd') {
        return FLAG_MD;
    }
    if (p[0] == 'm' && p[1] == 'f') {
        return FLAG_MF;
    }
    return FLAG_NONE;
}

static status_t append_custom(text_buffer_t *buffer, flag_t flag, va_list *args)
{
    switch (flag) {
    case FLAG_RO:
        return append_roman(buffer, va_arg(*args, int));
    case FLAG_ZR:
        return append_zeckendorf(buffer, va_arg(*args, unsigned int));
    case FLAG_CV_LOWER:
    case FLAG_CV_UPPER: {
        int value = va_arg(*args, int);
        int base = va_arg(*args, int);
        return append_in_base(buffer, value, base, flag == FLAG_CV_UPPER);
    }
    case FLAG_TO_LOWER:
    case FLAG_TO_UPPER: {
        const char *text = va_arg(*args, const char *);
        int base = va_arg(*args, int);
        return append_decimal_from_base(buffer, text, base, flag == FLAG_TO_UPPER);
    }
    case FLAG_MI: {
        int value = va_arg(*args, int);
        return append_dump(buffer, &value, sizeof(value));
    }
    case FLAG_MU: {
        unsigned int value = va_arg(*args, unsigned int);
        return append_dump(buffer, &value, sizeof(value));
    }
    case FLAG_MD: {
        double value = va_arg(*args, double);
        return append_dump(buffer, &value, sizeof(value));
    }
    case FLAG_MF: {
        float value = (float)va_arg(*args, double);
        return append_dump(buffer, &value, sizeof(value));
    }
    default:
        return STATUS_INVALID_FORMAT;
    }
}

static int spec_push(char *spec, size_t *length, const char *text)
{
    size_t text_length = strlen(text);
    if (*length + text_length + 1 > SPEC_SIZE) {
        return 0;
    }
    memcpy(spec + *length, text, text_length + 1);
    *length += text_length;
    return 1;
}

static int spec_push_char(char *spec, size_t *length, char c)
{
    char text[2] = {c, '\0'};
    return spec_push(spec, length, text);
}

static int spec_push_int(char *spec, size_t *length, int value)
{
    char text[16];
    snprintf(text, sizeof(text), "%d", value);
    return spec_push(spec, length, text);
}

static status_t parse_spec(const char **cursor, va_list *args, char *spec, length_t *length_mod, char *conversion)
{
    const char *p = *cursor;
    size_t length = 0;
    int ok = spec_push_char(spec, &length, '%');
    while (ok && *p != '\0' && strchr("-+ #0", *p) != NULL) {
        ok = spec_push_char(spec, &length, *p++);
    }
    if (ok && *p == '*') {
        ok = spec_push_int(spec, &length, va_arg(*args, int));
        p++;
    }
    while (ok && *p >= '0' && *p <= '9') {
        ok = spec_push_char(spec, &length, *p++);
    }
    if (ok && *p == '.') {
        p++;
        if (*p == '*') {
            int precision = va_arg(*args, int);
            p++;
            if (precision >= 0) {
                ok = spec_push_char(spec, &length, '.') && spec_push_int(spec, &length, precision);
            }
        } else {
            ok = spec_push_char(spec, &length, '.');
            while (ok && *p >= '0' && *p <= '9') {
                ok = spec_push_char(spec, &length, *p++);
            }
        }
    }
    *length_mod = LEN_NONE;
    if (p[0] == 'h' && p[1] == 'h') {
        *length_mod = LEN_HH;
        ok = ok && spec_push(spec, &length, "hh");
        p += 2;
    } else if (p[0] == 'l' && p[1] == 'l') {
        *length_mod = LEN_LL;
        ok = ok && spec_push(spec, &length, "ll");
        p += 2;
    } else if (*p == 'h' || *p == 'l' || *p == 'j' || *p == 'z' || *p == 't' || *p == 'L') {
        *length_mod = *p == 'h' ? LEN_H : *p == 'l' ? LEN_L : *p == 'j' ? LEN_J
                    : *p == 'z' ? LEN_Z : *p == 't' ? LEN_T : LEN_BIG_L;
        ok = ok && spec_push_char(spec, &length, *p);
        p++;
    }
    if (!ok || *p == '\0' || !spec_push_char(spec, &length, *p)) {
        return STATUS_INVALID_FORMAT;
    }
    *conversion = *p;
    *cursor = p + 1;
    return STATUS_OK;
}

static status_t append_signed(text_buffer_t *buffer, const char *spec, length_t length_mod, va_list *args)
{
    switch (length_mod) {
    case LEN_L:
        return append_formatted(buffer, spec, va_arg(*args, long));
    case LEN_LL:
        return append_formatted(buffer, spec, va_arg(*args, long long));
    case LEN_J:
        return append_formatted(buffer, spec, va_arg(*args, intmax_t));
    case LEN_Z:
        return append_formatted(buffer, spec, va_arg(*args, size_t));
    case LEN_T:
        return append_formatted(buffer, spec, va_arg(*args, ptrdiff_t));
    default:
        return append_formatted(buffer, spec, va_arg(*args, int));
    }
}

static status_t append_unsigned(text_buffer_t *buffer, const char *spec, length_t length_mod, va_list *args)
{
    switch (length_mod) {
    case LEN_L:
        return append_formatted(buffer, spec, va_arg(*args, unsigned long));
    case LEN_LL:
        return append_formatted(buffer, spec, va_arg(*args, unsigned long long));
    case LEN_J:
        return append_formatted(buffer, spec, va_arg(*args, uintmax_t));
    case LEN_Z:
        return append_formatted(buffer, spec, va_arg(*args, size_t));
    case LEN_T:
        return append_formatted(buffer, spec, va_arg(*args, ptrdiff_t));
    default:
        return append_formatted(buffer, spec, va_arg(*args, unsigned int));
    }
}

static status_t append_standard(text_buffer_t *buffer, const char **cursor, va_list *args)
{
    char spec[SPEC_SIZE];
    length_t length_mod = LEN_NONE;
    char conversion = '\0';
    status_t status = parse_spec(cursor, args, spec, &length_mod, &conversion);
    if (status != STATUS_OK) {
        return status;
    }
    switch (conversion) {
    case '%':
        return buffer_append_char(buffer, '%');
    case 'd':
    case 'i':
        return append_signed(buffer, spec, length_mod, args);
    case 'u':
    case 'o':
    case 'x':
    case 'X':
        return append_unsigned(buffer, spec, length_mod, args);
    case 'c':
        return append_formatted(buffer, spec, va_arg(*args, int));
    case 's':
        return append_formatted(buffer, spec, va_arg(*args, const char *));
    case 'p':
        return append_formatted(buffer, spec, va_arg(*args, void *));
    case 'f':
    case 'F':
    case 'e':
    case 'E':
    case 'g':
    case 'G':
    case 'a':
    case 'A':
        if (length_mod == LEN_BIG_L) {
            return append_formatted(buffer, spec, va_arg(*args, long double));
        }
        return append_formatted(buffer, spec, va_arg(*args, double));
    default:
        return STATUS_INVALID_FORMAT;
    }
}

static status_t format_into(text_buffer_t *buffer, const char *format, va_list *args)
{
    status_t status = buffer_reserve(buffer, 0);
    const char *p = format;
    while (status == STATUS_OK && *p != '\0') {
        if (*p != '%') {
            status = buffer_append_char(buffer, *p++);
            continue;
        }
        p++;
        flag_t flag = detect_flag(p);
        if (flag != FLAG_NONE) {
            status = append_custom(buffer, flag, args);
            p += 2;
        } else {
            status = append_standard(buffer, &p, args);
        }
    }
    return status;
}

int overfprintf(FILE *stream, const char *format, ...)
{
    if (stream == NULL || format == NULL) {
        return -1;
    }
    text_buffer_t buffer = {NULL, 0, 0};
    va_list args;
    va_start(args, format);
    status_t status = format_into(&buffer, format, &args);
    va_end(args);
    int written = -1;
    if (status == STATUS_OK && buffer.length <= (size_t)MAX_INT_VALUE
        && fwrite(buffer.data, 1, buffer.length, stream) == buffer.length) {
        written = (int)buffer.length;
    }
    free(buffer.data);
    return written;
}

int oversprintf(char *str, const char *format, ...)
{
    if (str == NULL || format == NULL) {
        return -1;
    }
    text_buffer_t buffer = {NULL, 0, 0};
    va_list args;
    va_start(args, format);
    status_t status = format_into(&buffer, format, &args);
    va_end(args);
    int written = -1;
    if (status == STATUS_OK && buffer.length <= (size_t)MAX_INT_VALUE) {
        memcpy(str, buffer.data, buffer.length + 1);
        written = (int)buffer.length;
    }
    free(buffer.data);
    return written;
}

int main(void)
{
    char text[256];
    int count = 0;

    count = overfprintf(stdout, "Roman: %Ro %Ro %Ro\n", 1994, 4, 3999);
    printf("written: %d\n", count);
    overfprintf(stdout, "Zeckendorf: %Zr %Zr %Zr\n", 9u, 1u, 100u);
    overfprintf(stdout, "Base: %Cv %CV %Cv %Cv %Cv\n", 255, 16, 255, 16, -255, 2, 35, 36, 99, 1);
    overfprintf(stdout, "To decimal: %to %TO %to\n", "ff", 16, "FF", 16, "-101", 2);
    overfprintf(stdout, "Dump int: %mi\n", 5);
    overfprintf(stdout, "Dump uint: %mu\n", 4294967295u);
    overfprintf(stdout, "Dump float: %mf\n", 1.5f);
    overfprintf(stdout, "Dump double: %md\n", 1.5);
    overfprintf(stdout, "Standard: [%5d] [%-6.2f] [%s] [%c] [%x] [%*d] [%ld] [%%]\n",
                42, 3.14159, "text", 'z', 255, 6, 7, 123456789L);

    count = oversprintf(text, "sprintf: %Ro | %Zr | %Cv | %mi | %d", 14, 12u, 31, 2, -1, 77);
    printf("%s\n", text);
    printf("length: %d\n", count);

    count = oversprintf(text, "bad value: %Ro", 0);
    printf("Ro(0) result: %d\n", count);
    count = oversprintf(text, "bad digit: %to", "xyz", 16);
    printf("to(xyz) result: %d\n", count);
    return 0;
}
