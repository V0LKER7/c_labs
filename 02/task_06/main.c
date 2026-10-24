#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SPEC_SIZE 48
#define MAX_ROMAN_LENGTH 32
#define MAX_ZECKENDORF_DIGITS 46

typedef enum {
    STATUS_OK,
    STATUS_INPUT_END,
    STATUS_MATCH_FAILURE,
    STATUS_INVALID_FORMAT
} status_t;

typedef enum {
    FLAG_NONE,
    FLAG_RO,
    FLAG_ZR,
    FLAG_CV_LOWER,
    FLAG_CV_UPPER
} flag_t;

typedef struct {
    FILE *stream;
    const char *text;
    size_t position;
} source_t;

static int source_peek(source_t *source)
{
    if (source->stream != NULL) {
        int c = fgetc(source->stream);
        if (c != EOF) {
            ungetc(c, source->stream);
        }
        return c;
    }
    unsigned char c = (unsigned char)source->text[source->position];
    return c == '\0' ? EOF : (int)c;
}

static void source_advance(source_t *source)
{
    if (source->stream != NULL) {
        fgetc(source->stream);
    } else if (source->text[source->position] != '\0') {
        source->position++;
    }
}

static void skip_spaces(source_t *source)
{
    int c = source_peek(source);
    while (c != EOF && isspace(c)) {
        source_advance(source);
        c = source_peek(source);
    }
}

static int roman_value(int c)
{
    switch (c) {
    case 'I':
        return 1;
    case 'V':
        return 5;
    case 'X':
        return 10;
    case 'L':
        return 50;
    case 'C':
        return 100;
    case 'D':
        return 500;
    case 'M':
        return 1000;
    default:
        return 0;
    }
}

static status_t scan_roman(source_t *source, int *value)
{
    char symbols[MAX_ROMAN_LENGTH];
    size_t count = 0;
    skip_spaces(source);
    int c = source_peek(source);
    if (c == EOF) {
        return STATUS_INPUT_END;
    }
    while (c != EOF && roman_value(c) > 0 && count < MAX_ROMAN_LENGTH) {
        symbols[count++] = (char)c;
        source_advance(source);
        c = source_peek(source);
    }
    if (count == 0) {
        return STATUS_MATCH_FAILURE;
    }
    int total = 0;
    int previous = 0;
    for (size_t i = count; i > 0; i--) {
        int current = roman_value(symbols[i - 1]);
        total += current < previous ? -current : current;
        previous = current;
    }
    *value = total;
    return STATUS_OK;
}

static status_t scan_zeckendorf(source_t *source, unsigned int *value)
{
    const unsigned long long max_value = ~0u;
    unsigned long long current = 1;
    unsigned long long next = 2;
    unsigned long long total = 0;
    int previous_one = 0;
    skip_spaces(source);
    for (size_t index = 0; index < MAX_ZECKENDORF_DIGITS; index++) {
        int c = source_peek(source);
        if (c == EOF) {
            return index == 0 ? STATUS_INPUT_END : STATUS_MATCH_FAILURE;
        }
        if (c != '0' && c != '1') {
            return STATUS_MATCH_FAILURE;
        }
        source_advance(source);
        if (c == '1' && previous_one) {
            if (total > max_value) {
                return STATUS_MATCH_FAILURE;
            }
            *value = (unsigned int)total;
            return STATUS_OK;
        }
        if (c == '1') {
            total += current;
        }
        previous_one = c == '1';
        unsigned long long following = current + next;
        current = next;
        next = following;
    }
    return STATUS_MATCH_FAILURE;
}

static int digit_value(int c, int uppercase)
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

static status_t scan_in_base(source_t *source, int base, int uppercase, int *value)
{
    unsigned int result = 0;
    int negative = 0;
    size_t digits = 0;
    if (base < 2 || base > 36) {
        base = 10;
    }
    skip_spaces(source);
    int c = source_peek(source);
    if (c == EOF) {
        return STATUS_INPUT_END;
    }
    if (c == '-' || c == '+') {
        negative = c == '-';
        source_advance(source);
        c = source_peek(source);
    }
    while (c != EOF && digit_value(c, uppercase) >= 0 && digit_value(c, uppercase) < base) {
        result = result * (unsigned int)base + (unsigned int)digit_value(c, uppercase);
        digits++;
        source_advance(source);
        c = source_peek(source);
    }
    if (digits == 0) {
        return STATUS_MATCH_FAILURE;
    }
    *value = negative ? (int)(0u - result) : (int)result;
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
    return FLAG_NONE;
}

static int spec_push_char(char *spec, size_t *length, char c)
{
    if (*length + 2 > SPEC_SIZE - 3) {
        return 0;
    }
    spec[(*length)++] = c;
    return 1;
}

static status_t build_spec(const char **cursor, char *spec, int *takes_argument)
{
    const char *p = *cursor;
    size_t length = 0;
    int ok = spec_push_char(spec, &length, '%');
    *takes_argument = 1;
    if (*p == '*') {
        *takes_argument = 0;
        ok = ok && spec_push_char(spec, &length, *p++);
    }
    while (ok && *p >= '0' && *p <= '9') {
        ok = spec_push_char(spec, &length, *p++);
    }
    while (ok && *p != '\0' && strchr("hlLjzt", *p) != NULL) {
        ok = spec_push_char(spec, &length, *p++);
    }
    if (!ok || *p == '\0' || *p == 'n') {
        return STATUS_INVALID_FORMAT;
    }
    if (*p == '[') {
        ok = spec_push_char(spec, &length, *p++);
        if (ok && *p == '^') {
            ok = spec_push_char(spec, &length, *p++);
        }
        if (ok && *p == ']') {
            ok = spec_push_char(spec, &length, *p++);
        }
        while (ok && *p != '\0' && *p != ']') {
            ok = spec_push_char(spec, &length, *p++);
        }
        if (!ok || *p != ']') {
            return STATUS_INVALID_FORMAT;
        }
    } else if (*p == '%') {
        *takes_argument = 0;
    }
    ok = spec_push_char(spec, &length, *p++);
    spec[length++] = '%';
    spec[length++] = 'n';
    spec[length] = '\0';
    *cursor = p;
    return ok ? STATUS_OK : STATUS_INVALID_FORMAT;
}

static status_t scan_standard(source_t *source, const char **cursor, va_list *args, int *assigned)
{
    char spec[SPEC_SIZE];
    int takes_argument = 1;
    status_t status = build_spec(cursor, spec, &takes_argument);
    if (status != STATUS_OK) {
        return status;
    }
    void *target = takes_argument ? va_arg(*args, void *) : NULL;
    int consumed = -1;
    int result = 0;
    if (source->stream != NULL) {
        result = takes_argument ? fscanf(source->stream, spec, target, &consumed)
                                : fscanf(source->stream, spec, &consumed);
    } else {
        const char *rest = source->text + source->position;
        result = takes_argument ? sscanf(rest, spec, target, &consumed) : sscanf(rest, spec, &consumed);
    }
    if (consumed < 0) {
        return result == EOF ? STATUS_INPUT_END : STATUS_MATCH_FAILURE;
    }
    if (source->stream == NULL) {
        source->position += (size_t)consumed;
    }
    if (takes_argument) {
        (*assigned)++;
    }
    return STATUS_OK;
}

static status_t scan_custom(source_t *source, flag_t flag, va_list *args)
{
    status_t status = STATUS_OK;
    if (flag == FLAG_RO) {
        int *target = va_arg(*args, int *);
        int value = 0;
        status = scan_roman(source, &value);
        if (status == STATUS_OK) {
            *target = value;
        }
    } else if (flag == FLAG_ZR) {
        unsigned int *target = va_arg(*args, unsigned int *);
        unsigned int value = 0;
        status = scan_zeckendorf(source, &value);
        if (status == STATUS_OK) {
            *target = value;
        }
    } else {
        int *target = va_arg(*args, int *);
        int base = va_arg(*args, int);
        int value = 0;
        status = scan_in_base(source, base, flag == FLAG_CV_UPPER, &value);
        if (status == STATUS_OK) {
            *target = value;
        }
    }
    return status;
}

static status_t scan_literal(source_t *source, char expected)
{
    int c = source_peek(source);
    if (c == EOF) {
        return STATUS_INPUT_END;
    }
    if (c != (unsigned char)expected) {
        return STATUS_MATCH_FAILURE;
    }
    source_advance(source);
    return STATUS_OK;
}

static status_t scan_format(source_t *source, const char *format, va_list *args, int *assigned)
{
    status_t status = STATUS_OK;
    const char *p = format;
    while (status == STATUS_OK && *p != '\0') {
        if (isspace((unsigned char)*p)) {
            skip_spaces(source);
            p++;
        } else if (*p != '%') {
            status = scan_literal(source, *p++);
        } else {
            p++;
            flag_t flag = detect_flag(p);
            if (flag != FLAG_NONE) {
                status = scan_custom(source, flag, args);
                if (status == STATUS_OK) {
                    (*assigned)++;
                }
                p += 2;
            } else {
                status = scan_standard(source, &p, args, assigned);
            }
        }
    }
    return status;
}

static int result_code(status_t status, int assigned)
{
    if (status == STATUS_INVALID_FORMAT) {
        return EOF;
    }
    if (status == STATUS_INPUT_END && assigned == 0) {
        return EOF;
    }
    return assigned;
}

int overfscanf(FILE *stream, const char *format, ...)
{
    if (stream == NULL || format == NULL) {
        return EOF;
    }
    source_t source = {stream, NULL, 0};
    int assigned = 0;
    va_list args;
    va_start(args, format);
    status_t status = scan_format(&source, format, &args, &assigned);
    va_end(args);
    return result_code(status, assigned);
}

int oversscanf(const char *str, const char *format, ...)
{
    if (str == NULL || format == NULL) {
        return EOF;
    }
    source_t source = {NULL, str, 0};
    int assigned = 0;
    va_list args;
    va_start(args, format);
    status_t status = scan_format(&source, format, &args, &assigned);
    va_end(args);
    return result_code(status, assigned);
}

int main(void)
{
    int roman = 0;
    unsigned int zeckendorf = 0;
    int lower = 0;
    int upper = 0;
    int plain = 0;
    char word[32] = "";

    int count = oversscanf("MCMXCIV 100011 -ff 7F 42 hello", "%Ro %Zr %Cv %CV %d %31s",
                           &roman, &zeckendorf, &lower, 16, &upper, 16, &plain, word);
    printf("assigned: %d\n", count);
    printf("roman=%d zeckendorf=%u lower=%d upper=%d plain=%d word=%s\n",
           roman, zeckendorf, lower, upper, plain, word);

    int binary = 0;
    int invalid_base = 0;
    count = oversscanf("1011 77", "%Cv %Cv", &binary, 2, &invalid_base, 99);
    printf("assigned: %d binary=%d invalid_base_as_10=%d\n", count, binary, invalid_base);

    int number = 0;
    int skipped_roman = 0;
    count = oversscanf("x=XLII;", "x=%Ro;", &skipped_roman);
    printf("assigned: %d literal_match_roman=%d\n", count, skipped_roman);
    count = oversscanf("abc", "%Ro", &number);
    printf("no roman digits: %d\n", count);
    count = oversscanf("", "%Ro", &number);
    printf("empty input: %d\n", count);

    FILE *file = tmpfile();
    if (file == NULL) {
        fprintf(stderr, "Error: cannot create temporary file\n");
        return 1;
    }
    fputs("MMXXVI 1011 zz 3.5 end\n", file);
    rewind(file);
    unsigned int fib = 0;
    int base36 = 0;
    double real = 0.0;
    char tail[16] = "";
    roman = 0;
    count = overfscanf(file, "%Ro %Zr %Cv %lf %15s", &roman, &fib, &base36, 36, &real, tail);
    printf("file assigned: %d roman=%d zeckendorf=%u base36=%d real=%.1f tail=%s\n",
           count, roman, fib, base36, real, tail);
    fclose(file);
    return 0;
}
