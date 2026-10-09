#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#define INITIAL_LEXEME_CAPACITY 32
#define MIN_BASE 2
#define MAX_BASE 36

typedef long long ll;

typedef enum {
    OK = 0,
    ERR_ARGS_COUNT,
    ERR_FILE_OPEN,
    ERR_SAME_FILE,
    ERR_MEMORY,
    ERR_WRITE,
    ERR_TOKEN,
    ERR_OVERFLOW,
    END_OF_FILE
} status_t;

typedef struct {
    int negative;
    const char *digits;
    int base;
} number_info_t;

static int is_separator(const int ch)
{
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

static int digit_value(const int ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'A' && ch <= 'Z')
        return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'z')
        return ch - 'a' + 10;
    return -1;
}

static status_t read_lexeme(FILE *file, char **lexeme)
{
    size_t capacity = INITIAL_LEXEME_CAPACITY;
    size_t size = 0;
    char *buffer;
    int ch;

    do {
        ch = fgetc(file);
    } while (ch != EOF && is_separator(ch));
    if (ch == EOF)
        return END_OF_FILE;
    buffer = malloc(capacity);
    if (buffer == NULL)
        return ERR_MEMORY;
    while (ch != EOF && !is_separator(ch)) {
        if (size + 1 >= capacity) {
            char *bigger = realloc(buffer, capacity * 2);

            if (bigger == NULL) {
                free(buffer);
                return ERR_MEMORY;
            }
            buffer = bigger;
            capacity *= 2;
        }
        buffer[size++] = (char)ch;
        ch = fgetc(file);
    }
    buffer[size] = '\0';
    *lexeme = buffer;
    return OK;
}

static status_t analyze_number(const char *token, number_info_t *info)
{
    const char *p = token;
    int max_digit = 0;

    info->negative = 0;
    if (*p == '-' || *p == '+') {
        info->negative = *p == '-';
        p++;
    }
    if (*p == '\0')
        return ERR_TOKEN;
    for (const char *q = p; *q != '\0'; q++) {
        int digit = digit_value(*q);

        if (digit < 0)
            return ERR_TOKEN;
        if (digit > max_digit)
            max_digit = digit;
    }
    while (*p == '0' && p[1] != '\0')
        p++;
    if (p[0] == '0')
        info->negative = 0;
    info->digits = p;
    info->base = max_digit + 1 < MIN_BASE ? MIN_BASE : max_digit + 1;
    return OK;
}

static status_t to_decimal(const number_info_t *info, ll *value)
{
    unsigned long long magnitude = 0;

    for (const char *p = info->digits; *p != '\0'; p++) {
        unsigned long long digit = (unsigned long long)digit_value(*p);

        if (magnitude > (LLONG_MAX - digit) / (unsigned long long)info->base)
            return ERR_OVERFLOW;
        magnitude = magnitude * (unsigned long long)info->base + digit;
    }
    *value = info->negative ? -(ll)magnitude : (ll)magnitude;
    return OK;
}

static status_t write_result(FILE *output, const number_info_t *info, const ll value)
{
    if (info->negative && fputc('-', output) == EOF)
        return ERR_WRITE;
    for (const char *p = info->digits; *p != '\0'; p++)
        if (fputc(toupper((unsigned char)*p), output) == EOF)
            return ERR_WRITE;
    if (fprintf(output, " %d %lld\n", info->base, value) < 0)
        return ERR_WRITE;
    return OK;
}

static status_t process_file(FILE *input, FILE *output)
{
    char *token;
    status_t status;

    while ((status = read_lexeme(input, &token)) == OK) {
        number_info_t info;
        ll value;

        status = analyze_number(token, &info);
        if (status == OK)
            status = to_decimal(&info, &value);
        if (status == OK)
            status = write_result(output, &info, value);
        free(token);
        if (status != OK)
            return status;
    }
    return status == END_OF_FILE ? OK : status;
}

static status_t run(const char *input_path, const char *output_path)
{
    FILE *input;
    FILE *output;
    status_t status;

    if (strcmp(input_path, output_path) == 0)
        return ERR_SAME_FILE;
    input = fopen(input_path, "r");
    if (input == NULL)
        return ERR_FILE_OPEN;
    output = fopen(output_path, "w");
    if (output == NULL) {
        fclose(input);
        return ERR_FILE_OPEN;
    }
    status = process_file(input, output);
    fclose(input);
    if (fclose(output) != 0 && status == OK)
        status = ERR_WRITE;
    return status;
}

static const char *status_message(const status_t status)
{
    switch (status) {
    case ERR_ARGS_COUNT: return "Usage: program <input> <output>";
    case ERR_FILE_OPEN: return "Cannot open file";
    case ERR_SAME_FILE: return "Input and output files must differ";
    case ERR_MEMORY: return "Memory allocation failed";
    case ERR_WRITE: return "Write error";
    case ERR_TOKEN: return "Input contains an invalid number";
    case ERR_OVERFLOW: return "Number does not fit into long long";
    default: return "";
    }
}

int main(int argc, char *argv[])
{
    status_t status;

    if (argc != 3) {
        fprintf(stderr, "%s\n", status_message(ERR_ARGS_COUNT));
        return 1;
    }
    status = run(argv[1], argv[2]);
    if (status != OK) {
        fprintf(stderr, "%s\n", status_message(status));
        return 1;
    }
    return 0;
}
