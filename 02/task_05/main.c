#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SPEC_SIZE 64
#define SPEC_LIMIT 48

typedef enum {
    STATUS_OK,
    STATUS_NULL_POINTER,
    STATUS_NO_MEMORY,
    STATUS_INVALID_FORMAT,
    STATUS_INVALID_VALUE
} Status;

typedef enum {
    LENGTH_NONE,
    LENGTH_L,
    LENGTH_LL,
    LENGTH_J,
    LENGTH_Z,
    LENGTH_T,
    LENGTH_BIG_L
} Length;

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} Buffer;

static Status buffer_init(Buffer *buffer)
{
    buffer->capacity = 64;
    buffer->length = 0;
    buffer->data = malloc(buffer->capacity);
    if (buffer->data == NULL) {
        return STATUS_NO_MEMORY;
    }
    buffer->data[0] = '\0';
    return STATUS_OK;
}

static Status buffer_reserve(Buffer *buffer, size_t extra)
{
    size_t needed = buffer->length + extra + 1;
    if (needed <= buffer->capacity) {
        return STATUS_OK;
    }
    size_t capacity = buffer->capacity;
    while (capacity < needed) {
        capacity *= 2;
    }
    char *temp = realloc(buffer->data, capacity);
    if (temp == NULL) {
        return STATUS_NO_MEMORY;
    }
    buffer->data = temp;
    buffer->capacity = capacity;
    return STATUS_OK;
}

static Status buffer_append_char(Buffer *buffer, char symbol)
{
    Status status = buffer_reserve(buffer, 1);
    if (status != STATUS_OK) {
        return status;
    }
    buffer->data[buffer->length++] = symbol;
    buffer->data[buffer->length] = '\0';
    return STATUS_OK;
}

static Status buffer_append_text(Buffer *buffer, const char *text)
{
    for (; *text != '\0'; text++) {
        Status status = buffer_append_char(buffer, *text);
        if (status != STATUS_OK) {
            return status;
        }
    }
    return STATUS_OK;
}

static Status append_formatted(Buffer *buffer, const char *format, ...)
{
    va_list args;
    va_list copy;
    va_start(args, format);
    va_copy(copy, args);
    int needed = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    Status status = STATUS_INVALID_VALUE;
    if (needed >= 0) {
        status = buffer_reserve(buffer, (size_t)needed);
        if (status == STATUS_OK) {
            vsnprintf(buffer->data + buffer->length, (size_t)needed + 1, format, args);
            buffer->length += (size_t)needed;
        }
    }
    va_end(args);
    return status;
}

static int normalize_base(int base)
{
    return (base < 2 || base > 36) ? 10 : base;
}

static Status append_roman(Buffer *buffer, int value)
{
    static const int values[] = {1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1};
    static const char *symbols[] = {"M", "CM", "D", "CD", "C", "XC", "L", "XL", "X", "IX", "V", "IV", "I"};
    long long number = value;
    if (number < 0) {
        number = -number;
    }
    if (number == 0 || number > 3999) {
        return STATUS_INVALID_VALUE;
    }
    if (value < 0) {
        Status status = buffer_append_char(buffer, '-');
        if (status != STATUS_OK) {
            return status;
        }
    }
    for (size_t i = 0; i < 13; i++) {
        while (number >= values[i]) {
            Status status = buffer_append_text(buffer, symbols[i]);
            if (status != STATUS_OK) {
                return status;
            }
            number -= values[i];
        }
    }
    return STATUS_OK;
}

static Status append_zeckendorf(Buffer *buffer, unsigned int value)
{
    unsigned long long fib[48];
    int bits[48] = {0};
    fib[0] = 1;
    fib[1] = 2;
    for (size_t i = 2; i < 48; i++) {
        fib[i] = fib[i - 1] + fib[i - 2];
    }
    int top = -1;
    for (int i = 0; i < 48; i++) {
        if (fib[i] <= value) {
            top = i;
        }
    }
    unsigned long long rest = value;
    for (int i = top; i >= 0; i--) {
        if (fib[i] <= rest) {
            bits[i] = 1;
            rest -= fib[i];
        }
    }
    Status status = STATUS_OK;
    if (top < 0) {
        status = buffer_append_char(buffer, '0');
    }
    for (int i = 0; i <= top && status == STATUS_OK; i++) {
        status = buffer_append_char(buffer, (char)('0' + bits[i]));
    }
    if (status != STATUS_OK) {
        return status;
    }
    return buffer_append_char(buffer, '1');
}

static Status append_in_base(Buffer *buffer, int value, int base, int uppercase)
{
    const char *digits = uppercase ? "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                   : "0123456789abcdefghijklmnopqrstuvwxyz";
    base = normalize_base(base);
    long long number = value;
    int negative = number < 0;
    if (negative) {
        number = -number;
    }
    char reversed[70];
    size_t length = 0;
    do {
        reversed[length++] = digits[number % base];
        number /= base;
    } while (number > 0);
    Status status = negative ? buffer_append_char(buffer, '-') : STATUS_OK;
    while (length > 0 && status == STATUS_OK) {
        status = buffer_append_char(buffer, reversed[--length]);
    }
    return status;
}

static Status append_to_decimal(Buffer *buffer, const char *text, int base)
{
    if (text == NULL) {
        return STATUS_NULL_POINTER;
    }
    long long value = strtoll(text, NULL, normalize_base(base));
    return append_formatted(buffer, "%lld", value);
}

static Status append_dump(Buffer *buffer, const void *memory, size_t size)
{
    const unsigned char *bytes = memory;
    for (size_t i = 0; i < size; i++) {
        for (int bit = 7; bit >= 0; bit--) {
            Status status = buffer_append_char(buffer, (char)('0' + ((bytes[i] >> bit) & 1)));
            if (status != STATUS_OK) {
                return status;
            }
        }
        if (i + 1 < size) {
            Status status = buffer_append_char(buffer, ' ');
            if (status != STATUS_OK) {
                return status;
            }
        }
    }
    return STATUS_OK;
}

static Status copy_number(char *spec, size_t *size, const char **position, va_list *args)
{
    if (**position == '*') {
        int number = va_arg(*args, int);
        int written = snprintf(spec + *size, SPEC_SIZE - *size, "%d", number);
        if (written < 0 || (size_t)written >= SPEC_SIZE - *size) {
            return STATUS_INVALID_FORMAT;
        }
        *size += (size_t)written;
        (*position)++;
        return STATUS_OK;
    }
    while (isdigit((unsigned char)**position)) {
        if (*size >= SPEC_LIMIT) {
            return STATUS_INVALID_FORMAT;
        }
        spec[(*size)++] = *(*position)++;
    }
    return STATUS_OK;
}

static Status format_signed(Buffer *buffer, const char *spec, Length length, va_list *args)
{
    switch (length) {
        case LENGTH_L:
            return append_formatted(buffer, spec, va_arg(*args, long));
        case LENGTH_LL:
            return append_formatted(buffer, spec, va_arg(*args, long long));
        case LENGTH_J:
            return append_formatted(buffer, spec, va_arg(*args, intmax_t));
        case LENGTH_Z:
        case LENGTH_T:
            return append_formatted(buffer, spec, va_arg(*args, ptrdiff_t));
        case LENGTH_BIG_L:
            return STATUS_INVALID_FORMAT;
        default:
            return append_formatted(buffer, spec, va_arg(*args, int));
    }
}

static Status format_unsigned(Buffer *buffer, const char *spec, Length length, va_list *args)
{
    switch (length) {
        case LENGTH_L:
            return append_formatted(buffer, spec, va_arg(*args, unsigned long));
        case LENGTH_LL:
            return append_formatted(buffer, spec, va_arg(*args, unsigned long long));
        case LENGTH_J:
            return append_formatted(buffer, spec, va_arg(*args, uintmax_t));
        case LENGTH_Z:
        case LENGTH_T:
            return append_formatted(buffer, spec, va_arg(*args, size_t));
        case LENGTH_BIG_L:
            return STATUS_INVALID_FORMAT;
        default:
            return append_formatted(buffer, spec, va_arg(*args, unsigned int));
    }
}

static Length parse_length(const char **position)
{
    const char *p = *position;
    Length length = LENGTH_NONE;
    if (*p == 'h') {
        p++;
        if (*p == 'h') {
            p++;
        }
    } else if (*p == 'l') {
        p++;
        if (*p == 'l') {
            p++;
            length = LENGTH_LL;
        } else {
            length = LENGTH_L;
        }
    } else if (*p == 'j') {
        p++;
        length = LENGTH_J;
    } else if (*p == 'z') {
        p++;
        length = LENGTH_Z;
    } else if (*p == 't') {
        p++;
        length = LENGTH_T;
    } else if (*p == 'L') {
        p++;
        length = LENGTH_BIG_L;
    }
    return length;
}

static Status format_standard(Buffer *buffer, const char **position, va_list *args)
{
    char spec[SPEC_SIZE];
    size_t size = 0;
    const char *p = *position;
    spec[size++] = *p++;
    while (*p != '\0' && strchr("-+ #0", *p) != NULL) {
        if (size >= SPEC_LIMIT) {
            return STATUS_INVALID_FORMAT;
        }
        spec[size++] = *p++;
    }
    Status status = copy_number(spec, &size, &p, args);
    if (status != STATUS_OK) {
        return status;
    }
    if (*p == '.') {
        spec[size++] = *p++;
        status = copy_number(spec, &size, &p, args);
        if (status != STATUS_OK) {
            return status;
        }
    }
    const char *length_start = p;
    Length length = parse_length(&p);
    if ((size_t)(p - length_start) + size >= SPEC_LIMIT) {
        return STATUS_INVALID_FORMAT;
    }
    while (length_start < p) {
        spec[size++] = *length_start++;
    }
    char conversion = *p;
    if (conversion == '\0') {
        return STATUS_INVALID_FORMAT;
    }
    spec[size++] = conversion;
    spec[size] = '\0';
    *position = p + 1;

    switch (conversion) {
        case 'd':
        case 'i':
            return format_signed(buffer, spec, length, args);
        case 'u':
        case 'o':
        case 'x':
        case 'X':
            return format_unsigned(buffer, spec, length, args);
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
            if (length == LENGTH_BIG_L) {
                return append_formatted(buffer, spec, va_arg(*args, long double));
            }
            return append_formatted(buffer, spec, va_arg(*args, double));
        case '%':
            return buffer_append_char(buffer, '%');
        default:
            return STATUS_INVALID_FORMAT;
    }
}

static int is_flag(const char *position, char first, char second)
{
    return position[1] == first && position[2] == second;
}

static Status handle_specifier(Buffer *buffer, const char **position, va_list *args)
{
    const char *p = *position;
    if (is_flag(p, 'R', 'o')) {
        int value = va_arg(*args, int);
        *position += 3;
        return append_roman(buffer, value);
    }
    if (is_flag(p, 'Z', 'r')) {
        unsigned int value = va_arg(*args, unsigned int);
        *position += 3;
        return append_zeckendorf(buffer, value);
    }
    if (is_flag(p, 'C', 'v') || is_flag(p, 'C', 'V')) {
        int uppercase = p[2] == 'V';
        int value = va_arg(*args, int);
        int base = va_arg(*args, int);
        *position += 3;
        return append_in_base(buffer, value, base, uppercase);
    }
    if (is_flag(p, 't', 'o') || is_flag(p, 'T', 'O')) {
        const char *text = va_arg(*args, const char *);
        int base = va_arg(*args, int);
        *position += 3;
        return append_to_decimal(buffer, text, base);
    }
    if (is_flag(p, 'm', 'i')) {
        int value = va_arg(*args, int);
        *position += 3;
        return append_dump(buffer, &value, sizeof value);
    }
    if (is_flag(p, 'm', 'u')) {
        unsigned int value = va_arg(*args, unsigned int);
        *position += 3;
        return append_dump(buffer, &value, sizeof value);
    }
    if (is_flag(p, 'm', 'd')) {
        double value = va_arg(*args, double);
        *position += 3;
        return append_dump(buffer, &value, sizeof value);
    }
    if (is_flag(p, 'm', 'f')) {
        float value = (float)va_arg(*args, double);
        *position += 3;
        return append_dump(buffer, &value, sizeof value);
    }
    return format_standard(buffer, position, args);
}

static Status process_format(Buffer *buffer, const char *format, va_list *args)
{
    const char *position = format;
    while (*position != '\0') {
        Status status;
        if (*position != '%') {
            status = buffer_append_char(buffer, *position);
            position++;
        } else {
            status = handle_specifier(buffer, &position, args);
        }
        if (status != STATUS_OK) {
            return status;
        }
    }
    return STATUS_OK;
}

int overfprintf(FILE *stream, const char *format, ...)
{
    if (stream == NULL || format == NULL) {
        return -1;
    }
    Buffer buffer;
    if (buffer_init(&buffer) != STATUS_OK) {
        return -1;
    }
    va_list args;
    va_start(args, format);
    Status status = process_format(&buffer, format, &args);
    va_end(args);
    int result = -1;
    if (status == STATUS_OK && fwrite(buffer.data, 1, buffer.length, stream) == buffer.length) {
        result = (int)buffer.length;
    }
    free(buffer.data);
    return result;
}

int oversprintf(char *destination, const char *format, ...)
{
    if (destination == NULL || format == NULL) {
        return -1;
    }
    Buffer buffer;
    if (buffer_init(&buffer) != STATUS_OK) {
        return -1;
    }
    va_list args;
    va_start(args, format);
    Status status = process_format(&buffer, format, &args);
    va_end(args);
    int result = -1;
    if (status == STATUS_OK) {
        for (size_t i = 0; i <= buffer.length; i++) {
            destination[i] = buffer.data[i];
        }
        result = (int)buffer.length;
    }
    free(buffer.data);
    return result;
}

int main(void)
{
    char text[256];
    overfprintf(stdout, "Ro: %Ro %Ro %Ro %Ro\n", 1994, 4, 3999, -14);
    overfprintf(stdout, "Zr: %Zr %Zr %Zr %Zr\n", 1u, 4u, 9u, 100u);
    overfprintf(stdout, "Cv: %Cv %Cv %Cv %Cv\n", 255, 16, -255, 2, 35, 36, 10, 99);
    overfprintf(stdout, "CV: %CV %CV\n", 255, 16, 3735928559u > 0 ? 123456 : 0, 36);
    overfprintf(stdout, "to: %to %TO %to\n", "ff", 16, "ZZ", 36, "101", 2);
    overfprintf(stdout, "mi: %mi\n", 5);
    overfprintf(stdout, "mu: %mu\n", 4294967295u);
    overfprintf(stdout, "md: %md\n", 1.0);
    overfprintf(stdout, "mf: %mf\n", 1.0f);
    overfprintf(stdout, "std: [%5d] [%-5d] [%05.1f] [%s] [%c] [%x] [%ld] [%*d] [%%]\n",
                42, 42, 3.14159, "text", 'z', 255, 123456789L, 6, 7);
    int length = oversprintf(text, "sprintf: %Ro, %Zr, %Cv, %d", 2026, 20u, 100, 8, 5);
    printf("%s (length %d)\n", text, length);
    printf("invalid Ro(0): %d\n", oversprintf(text, "%Ro", 0));
    printf("invalid base: ");
    overfprintf(stdout, "%Cv\n", 100, 1);
    return 0;
}
