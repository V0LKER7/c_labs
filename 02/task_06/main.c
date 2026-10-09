#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SPEC_SIZE 64
#define ROMAN_MAX 32
#define FIB_COUNT 48

typedef enum {
    STATUS_OK,
    STATUS_FAIL
} Status;

typedef struct {
    FILE *file;
    const char *text;
    size_t position;
} Source;

static int source_get(Source *source)
{
    if (source->file != NULL) {
        return fgetc(source->file);
    }
    unsigned char symbol = (unsigned char)source->text[source->position];
    if (symbol == '\0') {
        return EOF;
    }
    source->position++;
    return symbol;
}

static void source_unget(Source *source, int symbol)
{
    if (symbol == EOF) {
        return;
    }
    if (source->file != NULL) {
        ungetc(symbol, source->file);
    } else {
        source->position--;
    }
}

static void skip_spaces(Source *source)
{
    int symbol;
    while ((symbol = source_get(source)) != EOF && isspace(symbol)) {
    }
    source_unget(source, symbol);
}

static int normalize_base(int base)
{
    return (base < 2 || base > 36) ? 10 : base;
}

static int roman_value(int symbol)
{
    switch (symbol) {
        case 'I': return 1;
        case 'V': return 5;
        case 'X': return 10;
        case 'L': return 50;
        case 'C': return 100;
        case 'D': return 500;
        case 'M': return 1000;
        default: return 0;
    }
}

static Status scan_roman(Source *source, int *result)
{
    skip_spaces(source);
    int values[ROMAN_MAX];
    size_t count = 0;
    while (count < ROMAN_MAX) {
        int symbol = source_get(source);
        if (symbol == EOF) {
            break;
        }
        int value = roman_value(symbol);
        if (value == 0) {
            source_unget(source, symbol);
            break;
        }
        values[count++] = value;
    }
    if (count == 0) {
        return STATUS_FAIL;
    }
    int total = 0;
    int previous = 0;
    for (size_t i = count; i > 0; i--) {
        int value = values[i - 1];
        if (value < previous) {
            total -= value;
        } else {
            total += value;
            previous = value;
        }
    }
    *result = total;
    return STATUS_OK;
}

static Status scan_zeckendorf(Source *source, unsigned int *result)
{
    skip_spaces(source);
    unsigned long long fib[FIB_COUNT];
    fib[0] = 1;
    fib[1] = 2;
    for (size_t i = 2; i < FIB_COUNT; i++) {
        fib[i] = fib[i - 1] + fib[i - 2];
    }
    unsigned long long total = 0;
    int previous = '0';
    size_t index = 0;
    for (;;) {
        int symbol = source_get(source);
        if (symbol != '0' && symbol != '1') {
            source_unget(source, symbol);
            return STATUS_FAIL;
        }
        if (symbol == '1' && previous == '1') {
            break;
        }
        if (index >= FIB_COUNT) {
            return STATUS_FAIL;
        }
        if (symbol == '1') {
            total += fib[index];
        }
        index++;
        previous = symbol;
    }
    *result = (unsigned int)total;
    return STATUS_OK;
}

static int digit_value(int symbol, int uppercase)
{
    if (isdigit(symbol)) {
        return symbol - '0';
    }
    if (!uppercase && symbol >= 'a' && symbol <= 'z') {
        return symbol - 'a' + 10;
    }
    if (uppercase && symbol >= 'A' && symbol <= 'Z') {
        return symbol - 'A' + 10;
    }
    return -1;
}

static Status scan_in_base(Source *source, int *result, int base, int uppercase)
{
    base = normalize_base(base);
    skip_spaces(source);
    int symbol = source_get(source);
    int sign = 1;
    if (symbol == '-' || symbol == '+') {
        if (symbol == '-') {
            sign = -1;
        }
        symbol = source_get(source);
    }
    long long total = 0;
    size_t digits = 0;
    while (symbol != EOF) {
        int value = digit_value(symbol, uppercase);
        if (value < 0 || value >= base) {
            break;
        }
        total = total * base + value;
        digits++;
        symbol = source_get(source);
    }
    source_unget(source, symbol);
    if (digits == 0) {
        return STATUS_FAIL;
    }
    *result = (int)(sign * total);
    return STATUS_OK;
}

static int spec_push(char *spec, size_t *size, const char **position)
{
    if (*size >= SPEC_SIZE - 4) {
        return 0;
    }
    spec[(*size)++] = *(*position)++;
    return 1;
}

static int build_spec(const char **format, char *spec)
{
    const char *p = *format;
    size_t size = 0;
    spec[size++] = *p++;
    while (isdigit((unsigned char)*p)) {
        if (!spec_push(spec, &size, &p)) {
            return 0;
        }
    }
    while (*p != '\0' && strchr("hlLjzt", *p) != NULL) {
        if (!spec_push(spec, &size, &p)) {
            return 0;
        }
    }
    if (*p == '\0' || *p == 'n' || *p == '*') {
        return 0;
    }
    if (*p == '[') {
        int ok = spec_push(spec, &size, &p);
        if (ok && *p == '^') {
            ok = spec_push(spec, &size, &p);
        }
        if (ok && *p == ']') {
            ok = spec_push(spec, &size, &p);
        }
        while (ok && *p != '\0' && *p != ']') {
            ok = spec_push(spec, &size, &p);
        }
        if (!ok || *p != ']') {
            return 0;
        }
    }
    if (!spec_push(spec, &size, &p)) {
        return 0;
    }
    spec[size] = '\0';
    *format = p;
    return 1;
}

static int scan_standard(Source *source, const char *spec, void *target)
{
    if (source->file != NULL) {
        return fscanf(source->file, spec, target) == 1;
    }
    char full_spec[SPEC_SIZE + 4];
    snprintf(full_spec, sizeof full_spec, "%s%%n", spec);
    int consumed = 0;
    if (sscanf(source->text + source->position, full_spec, target, &consumed) != 1) {
        return 0;
    }
    source->position += (size_t)consumed;
    return 1;
}

static int is_flag(const char *position, char first, char second)
{
    return position[1] == first && position[2] == second;
}

static int scan_conversion(Source *source, const char **position, va_list *args)
{
    const char *p = *position;
    if (is_flag(p, 'R', 'o')) {
        int *target = va_arg(*args, int *);
        *position += 3;
        return scan_roman(source, target) == STATUS_OK;
    }
    if (is_flag(p, 'Z', 'r')) {
        unsigned int *target = va_arg(*args, unsigned int *);
        *position += 3;
        return scan_zeckendorf(source, target) == STATUS_OK;
    }
    if (is_flag(p, 'C', 'v') || is_flag(p, 'C', 'V')) {
        int uppercase = p[2] == 'V';
        int *target = va_arg(*args, int *);
        int base = va_arg(*args, int);
        *position += 3;
        return scan_in_base(source, target, base, uppercase) == STATUS_OK;
    }
    char spec[SPEC_SIZE];
    if (!build_spec(position, spec)) {
        return 0;
    }
    void *target = va_arg(*args, void *);
    return scan_standard(source, spec, target);
}

static int scan_source(Source *source, const char *format, va_list *args)
{
    int assigned = 0;
    const char *position = format;
    while (*position != '\0') {
        if (isspace((unsigned char)*position)) {
            skip_spaces(source);
            position++;
        } else if (*position != '%') {
            int symbol = source_get(source);
            if (symbol != (unsigned char)*position) {
                source_unget(source, symbol);
                break;
            }
            position++;
        } else if (position[1] == '%') {
            skip_spaces(source);
            int symbol = source_get(source);
            if (symbol != '%') {
                source_unget(source, symbol);
                break;
            }
            position += 2;
        } else {
            if (!scan_conversion(source, &position, args)) {
                break;
            }
            assigned++;
        }
    }
    return assigned;
}

int overfscanf(FILE *stream, const char *format, ...)
{
    if (stream == NULL || format == NULL) {
        return -1;
    }
    Source source = {stream, NULL, 0};
    va_list args;
    va_start(args, format);
    int assigned = scan_source(&source, format, &args);
    va_end(args);
    return assigned;
}

int oversscanf(const char *text, const char *format, ...)
{
    if (text == NULL || format == NULL) {
        return -1;
    }
    Source source = {NULL, text, 0};
    va_list args;
    va_start(args, format);
    int assigned = scan_source(&source, format, &args);
    va_end(args);
    return assigned;
}

int main(void)
{
    int roman = 0;
    unsigned int zeckendorf = 0;
    int hex = 0;
    int upper = 0;
    int decimal = 0;
    char word[32] = "";

    int count = oversscanf("XIV 100011 ff 42 hello", "%Ro %Zr %Cv %d %s", &roman, &zeckendorf, &hex, 16, &decimal, word);
    printf("sscanf: count=%d roman=%d zeckendorf=%u hex=%d decimal=%d word=%s\n",
           count, roman, zeckendorf, hex, decimal, word);

    count = oversscanf("MCMXCIV 1011 -zz ZZ", "%Ro %Zr %Cv %CV", &roman, &zeckendorf, &hex, 36, &upper, 36);
    printf("sscanf: count=%d roman=%d zeckendorf=%u base36 lower=%d upper=%d\n", count, roman, zeckendorf, hex, upper);

    count = oversscanf("abc", "%Ro", &roman);
    printf("sscanf invalid roman: count=%d\n", count);

    count = oversscanf("101 777 12", "%Cv %Cv %Cv", &roman, 2, &hex, 8, &decimal, 99);
    printf("sscanf bases: count=%d bin=%d oct=%d default10=%d\n", count, roman, hex, decimal);

    FILE *file = fopen("sample_input.txt", "w+");
    if (file == NULL) {
        printf("Error: cannot create sample file\n");
        return 1;
    }
    fputs("CDXLIV 10101011 7F 3.5\n", file);
    rewind(file);
    double real = 0.0;
    count = overfscanf(file, "%Ro %Zr %CV %lf", &roman, &zeckendorf, &hex, 16, &real);
    printf("fscanf: count=%d roman=%d zeckendorf=%u hex=%d real=%.1f\n", count, roman, zeckendorf, hex, real);
    fclose(file);
    remove("sample_input.txt");
    return 0;
}
