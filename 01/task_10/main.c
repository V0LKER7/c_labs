#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MIN_BASE 2
#define MAX_BASE 36
#define LINE_BUFFER_SIZE 256
#define NUMBER_BUFFER_SIZE 70
#define STOP_WORD "Stop"

typedef enum {
    STATUS_OK,
    STATUS_INVALID_BASE,
    STATUS_INVALID_NUMBER,
    STATUS_LINE_TOO_LONG,
    STATUS_OVERFLOW,
    STATUS_END_OF_INPUT
} status_t;

static const char *status_message(status_t status)
{
    switch (status) {
    case STATUS_OK:
        return "ok";
    case STATUS_INVALID_BASE:
        return "base must be an integer from 2 to 36";
    case STATUS_INVALID_NUMBER:
        return "not a valid number in this base (digits above 9 must be uppercase)";
    case STATUS_LINE_TOO_LONG:
        return "input line is too long";
    case STATUS_OVERFLOW:
        return "number or sum does not fit into 64 bits";
    case STATUS_END_OF_INPUT:
        return "unexpected end of input";
    }
    return "unknown error";
}

static status_t read_line(char *buffer, size_t size)
{
    if (fgets(buffer, (int)size, stdin) == NULL) {
        return STATUS_END_OF_INPUT;
    }
    size_t length = strlen(buffer);
    if (length > 0 && buffer[length - 1] == '\n') {
        buffer[--length] = '\0';
    } else if (length + 1 == size) {
        int symbol = getchar();
        while (symbol != '\n' && symbol != EOF) {
            symbol = getchar();
        }
        return STATUS_LINE_TOO_LONG;
    }
    if (length > 0 && buffer[length - 1] == '\r') {
        buffer[length - 1] = '\0';
    }
    return STATUS_OK;
}

static status_t parse_base(const char *text, int *base)
{
    if (text == NULL || base == NULL || *text == '\0') {
        return STATUS_INVALID_BASE;
    }
    int value = 0;
    for (const char *symbol = text; *symbol != '\0'; symbol++) {
        if (*symbol < '0' || *symbol > '9' || value > MAX_BASE) {
            return STATUS_INVALID_BASE;
        }
        value = value * 10 + (*symbol - '0');
    }
    if (value < MIN_BASE || value > MAX_BASE) {
        return STATUS_INVALID_BASE;
    }
    *base = value;
    return STATUS_OK;
}

static int digit_value(char symbol)
{
    if (symbol >= '0' && symbol <= '9') {
        return symbol - '0';
    }
    if (symbol >= 'A' && symbol <= 'Z') {
        return symbol - 'A' + 10;
    }
    return -1;
}

static status_t parse_number(const char *text, int base, int64_t *value)
{
    if (text == NULL || value == NULL || base < MIN_BASE || base > MAX_BASE) {
        return STATUS_INVALID_NUMBER;
    }
    int negative = (text[0] == '-');
    const char *digits = negative ? text + 1 : text;
    if (*digits == '\0') {
        return STATUS_INVALID_NUMBER;
    }
    uint64_t magnitude = 0;
    for (const char *symbol = digits; *symbol != '\0'; symbol++) {
        int digit = digit_value(*symbol);
        if (digit < 0 || digit >= base) {
            return STATUS_INVALID_NUMBER;
        }
        if (magnitude > ((uint64_t)INT64_MAX - (uint64_t)digit) / (uint64_t)base) {
            return STATUS_OVERFLOW;
        }
        magnitude = magnitude * (uint64_t)base + (uint64_t)digit;
    }
    *value = negative ? -(int64_t)magnitude : (int64_t)magnitude;
    return STATUS_OK;
}

static uint64_t magnitude_of(int64_t value)
{
    return value < 0 ? (uint64_t)0 - (uint64_t)value : (uint64_t)value;
}

static status_t add_checked(int64_t first, int64_t second, int64_t *sum)
{
    if ((second > 0 && first > INT64_MAX - second) ||
        (second < 0 && first < INT64_MIN - second)) {
        return STATUS_OVERFLOW;
    }
    *sum = first + second;
    return STATUS_OK;
}

static status_t format_number(int64_t value, int base, char *buffer, size_t size)
{
    if (buffer == NULL || size < NUMBER_BUFFER_SIZE || base < MIN_BASE ||
        base > MAX_BASE) {
        return STATUS_INVALID_BASE;
    }
    const char alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char reversed[NUMBER_BUFFER_SIZE];
    size_t length = 0;
    uint64_t magnitude = magnitude_of(value);
    do {
        reversed[length++] = alphabet[magnitude % (uint64_t)base];
        magnitude /= (uint64_t)base;
    } while (magnitude > 0);
    size_t position = 0;
    if (value < 0) {
        buffer[position++] = '-';
    }
    while (length > 0) {
        buffer[position++] = reversed[--length];
    }
    buffer[position] = '\0';
    return STATUS_OK;
}

static status_t read_base(int *base)
{
    char line[LINE_BUFFER_SIZE];
    printf("Enter base (2..36): ");
    status_t status = read_line(line, sizeof(line));
    if (status == STATUS_LINE_TOO_LONG) {
        return STATUS_INVALID_BASE;
    }
    if (status != STATUS_OK) {
        return status;
    }
    return parse_base(line, base);
}

static status_t read_numbers(int base, int *count, int64_t *max_by_module,
                             int64_t *sum)
{
    char line[LINE_BUFFER_SIZE];
    *count = 0;
    *sum = 0;
    *max_by_module = 0;
    while (1) {
        printf("Enter number or Stop: ");
        status_t status = read_line(line, sizeof(line));
        if (status == STATUS_END_OF_INPUT || (status == STATUS_OK &&
                                              strcmp(line, STOP_WORD) == 0)) {
            return STATUS_OK;
        }
        int64_t value = 0;
        if (status == STATUS_OK) {
            status = parse_number(line, base, &value);
        }
        if (status == STATUS_OK) {
            status = add_checked(*sum, value, sum);
        }
        if (status != STATUS_OK) {
            printf("Skipped: %s\n", status_message(status));
            continue;
        }
        if (*count == 0 || magnitude_of(value) > magnitude_of(*max_by_module)) {
            *max_by_module = value;
        }
        (*count)++;
    }
}

static status_t print_in_bases(const char *title, int64_t value, int input_base)
{
    const int bases[] = {input_base, 9, 18, 27, 36};
    char buffer[NUMBER_BUFFER_SIZE];
    printf("%s:\n", title);
    for (size_t index = 0; index < sizeof(bases) / sizeof(bases[0]); index++) {
        status_t status = format_number(value, bases[index], buffer, sizeof(buffer));
        if (status != STATUS_OK) {
            return status;
        }
        printf("  base %2d: %s%s\n", bases[index], buffer,
               index == 0 ? " (input base)" : "");
    }
    return STATUS_OK;
}

int main(void)
{
    int base = 0;
    int count = 0;
    int64_t max_by_module = 0;
    int64_t sum = 0;
    status_t status = read_base(&base);
    if (status == STATUS_OK) {
        status = read_numbers(base, &count, &max_by_module, &sum);
    }
    if (status == STATUS_OK && count == 0) {
        printf("No numbers were entered\n");
        return 0;
    }
    if (status == STATUS_OK) {
        status = print_in_bases("Maximum by module", max_by_module, base);
    }
    if (status == STATUS_OK) {
        status = print_in_bases("Sum", sum, base);
    }
    if (status != STATUS_OK) {
        fprintf(stderr, "Error: %s\n", status_message(status));
        return 1;
    }
    return 0;
}
