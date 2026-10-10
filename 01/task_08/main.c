#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN_BASE 2
#define MAX_BASE 36
#define INITIAL_TOKEN_CAPACITY 32

typedef enum {
    STATUS_OK,
    STATUS_INVALID_ARGS_COUNT,
    STATUS_SAME_FILE,
    STATUS_OPEN_ERROR,
    STATUS_IO_ERROR,
    STATUS_MEMORY_ERROR,
    STATUS_INVALID_TOKEN,
    STATUS_OVERFLOW,
    STATUS_END_OF_FILE
} status_t;

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} token_t;

typedef struct {
    int negative;
    const char *digits;
    int min_base;
    uint64_t value;
} number_info_t;

static const char *status_message(status_t status)
{
    switch (status) {
    case STATUS_OK:
        return "ok";
    case STATUS_INVALID_ARGS_COUNT:
        return "wrong number of arguments";
    case STATUS_SAME_FILE:
        return "input and output paths must differ";
    case STATUS_OPEN_ERROR:
        return "cannot open file";
    case STATUS_IO_ERROR:
        return "input/output error";
    case STATUS_MEMORY_ERROR:
        return "memory allocation failed";
    case STATUS_INVALID_TOKEN:
        return "token is not a valid number in any base from 2 to 36";
    case STATUS_OVERFLOW:
        return "value does not fit into 64 bits even in the minimal base";
    case STATUS_END_OF_FILE:
        return "end of file";
    }
    return "unknown error";
}

static int is_separator(int symbol)
{
    return symbol == ' ' || symbol == '\t' || symbol == '\n' || symbol == '\r';
}

static status_t read_token(FILE *file, token_t *token)
{
    int symbol = fgetc(file);
    while (symbol != EOF && is_separator(symbol)) {
        symbol = fgetc(file);
    }
    if (symbol == EOF) {
        return ferror(file) ? STATUS_IO_ERROR : STATUS_END_OF_FILE;
    }
    token->length = 0;
    while (symbol != EOF && !is_separator(symbol)) {
        if (token->length + 2 > token->capacity) {
            size_t capacity = token->capacity == 0 ? INITIAL_TOKEN_CAPACITY
                                                   : token->capacity * 2;
            char *grown = realloc(token->data, capacity);
            if (grown == NULL) {
                return STATUS_MEMORY_ERROR;
            }
            token->data = grown;
            token->capacity = capacity;
        }
        token->data[token->length++] = (char)symbol;
        symbol = fgetc(file);
    }
    token->data[token->length] = '\0';
    return ferror(file) ? STATUS_IO_ERROR : STATUS_OK;
}

static int digit_value(char symbol)
{
    if (symbol >= '0' && symbol <= '9') {
        return symbol - '0';
    }
    if (symbol >= 'A' && symbol <= 'Z') {
        return symbol - 'A' + 10;
    }
    if (symbol >= 'a' && symbol <= 'z') {
        return symbol - 'a' + 10;
    }
    return -1;
}

static void normalize_case(char *text)
{
    for (; *text != '\0'; text++) {
        if (*text >= 'a' && *text <= 'z') {
            *text = (char)(*text - 'a' + 'A');
        }
    }
}

static status_t analyze_number(char *text, number_info_t *info)
{
    if (text == NULL || info == NULL) {
        return STATUS_INVALID_TOKEN;
    }
    normalize_case(text);
    info->negative = (text[0] == '-');
    char *digits = info->negative ? text + 1 : text;
    if (*digits == '\0') {
        return STATUS_INVALID_TOKEN;
    }
    int max_digit = 0;
    for (const char *symbol = digits; *symbol != '\0'; symbol++) {
        int digit = digit_value(*symbol);
        if (digit < 0) {
            return STATUS_INVALID_TOKEN;
        }
        if (digit > max_digit) {
            max_digit = digit;
        }
    }
    while (digits[0] == '0' && digits[1] != '\0') {
        digits++;
    }
    info->digits = digits;
    info->min_base = max_digit + 1 < MIN_BASE ? MIN_BASE : max_digit + 1;
    uint64_t value = 0;
    for (const char *symbol = digits; *symbol != '\0'; symbol++) {
        uint64_t digit = (uint64_t)digit_value(*symbol);
        if (value > (UINT64_MAX - digit) / (uint64_t)info->min_base) {
            return STATUS_OVERFLOW;
        }
        value = value * (uint64_t)info->min_base + digit;
    }
    info->value = value;
    if (value == 0) {
        info->negative = 0;
    }
    return STATUS_OK;
}

static status_t write_info(FILE *output, const number_info_t *info)
{
    int written = fprintf(output, "%s%s %d %s%llu\n", info->negative ? "-" : "",
                          info->digits, info->min_base, info->negative ? "-" : "",
                          (unsigned long long)info->value);
    return written < 0 ? STATUS_IO_ERROR : STATUS_OK;
}

static status_t process_file(FILE *input, FILE *output)
{
    token_t token = {NULL, 0, 0};
    status_t status = STATUS_OK;
    while (status == STATUS_OK) {
        status = read_token(input, &token);
        if (status != STATUS_OK) {
            break;
        }
        number_info_t info = {0, NULL, 0, 0};
        status = analyze_number(token.data, &info);
        if (status == STATUS_OK) {
            status = write_info(output, &info);
        } else {
            fprintf(stderr, "Bad token: %s\n", token.data);
        }
    }
    free(token.data);
    return status == STATUS_END_OF_FILE ? STATUS_OK : status;
}

static status_t run(const char *input_path, const char *output_path)
{
    if (strcmp(input_path, output_path) == 0) {
        return STATUS_SAME_FILE;
    }
    FILE *input = fopen(input_path, "rb");
    if (input == NULL) {
        return STATUS_OPEN_ERROR;
    }
    FILE *output = fopen(output_path, "wb");
    if (output == NULL) {
        fclose(input);
        return STATUS_OPEN_ERROR;
    }
    status_t status = process_file(input, output);
    if (fclose(output) != 0 && status == STATUS_OK) {
        status = STATUS_IO_ERROR;
    }
    fclose(input);
    return status;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input> <output>\n", argv[0]);
        return 1;
    }
    status_t status = run(argv[1], argv[2]);
    if (status != STATUS_OK) {
        fprintf(stderr, "Error: %s\n", status_message(status));
        return 1;
    }
    return 0;
}
