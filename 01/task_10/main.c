#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define LINE_SIZE 256
#define MIN_BASE 2
#define MAX_BASE 36
#define TEXT_SIZE 72
#define STOP_WORD "Stop"

typedef long long ll;
typedef unsigned long long ull;

typedef enum {
    OK = 0,
    ERR_INPUT,
    ERR_BASE,
    ERR_NUMBER,
    ERR_OVERFLOW,
    END_OF_INPUT
} status_t;

static ull magnitude_of(const ll value)
{
    return value < 0 ? (ull)(-(value + 1)) + 1 : (ull)value;
}

static status_t read_line(char *buffer, const size_t size)
{
    size_t length;

    if (fgets(buffer, (int)size, stdin) == NULL)
        return END_OF_INPUT;
    length = strlen(buffer);
    if (length > 0 && buffer[length - 1] == '\n') {
        buffer[length - 1] = '\0';
        return OK;
    }
    if (!feof(stdin)) {
        int ch;

        while ((ch = getchar()) != '\n' && ch != EOF)
            ;
        return ERR_INPUT;
    }
    return OK;
}

static status_t parse_base(const char *text, int *base)
{
    char *end;
    long value;

    if (*text == '\0')
        return ERR_BASE;
    value = strtol(text, &end, 10);
    if (*end != '\0' || value < MIN_BASE || value > MAX_BASE)
        return ERR_BASE;
    *base = (int)value;
    return OK;
}

static int digit_value(const char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'A' && ch <= 'Z')
        return ch - 'A' + 10;
    return -1;
}

static status_t parse_number(const char *text, const int base, ll *value)
{
    int negative = text[0] == '-';
    const char *p = negative ? text + 1 : text;
    ull magnitude = 0;

    if (*p == '\0')
        return ERR_NUMBER;
    for (; *p != '\0'; p++) {
        int digit = digit_value(*p);

        if (digit < 0 || digit >= base)
            return ERR_NUMBER;
        if (magnitude > (LLONG_MAX - (ull)digit) / (ull)base)
            return ERR_OVERFLOW;
        magnitude = magnitude * (ull)base + (ull)digit;
    }
    *value = negative ? -(ll)magnitude : (ll)magnitude;
    return OK;
}

static status_t add_checked(const ll first, const ll second, ll *sum)
{
    if ((second > 0 && first > LLONG_MAX - second) || (second < 0 && first < LLONG_MIN - second))
        return ERR_OVERFLOW;
    *sum = first + second;
    return OK;
}

static status_t to_base_text(const ll value, const int base, char *buffer, const size_t size)
{
    static const char digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char reversed[TEXT_SIZE];
    ull magnitude = magnitude_of(value);
    size_t length = 0;
    size_t position = 0;

    do {
        reversed[length++] = digits[magnitude % (ull)base];
        magnitude /= (ull)base;
    } while (magnitude > 0);
    if (length + 2 > size)
        return ERR_OVERFLOW;
    if (value < 0)
        buffer[position++] = '-';
    while (length > 0)
        buffer[position++] = reversed[--length];
    buffer[position] = '\0';
    return OK;
}

static status_t read_numbers(const int base, ll *max_value, ll *sum, size_t *count)
{
    char line[LINE_SIZE];
    status_t status;

    *count = 0;
    *sum = 0;
    *max_value = 0;
    for (;;) {
        ll value;

        status = read_line(line, sizeof(line));
        if (status == END_OF_INPUT || (status == OK && strcmp(line, STOP_WORD) == 0))
            return OK;
        if (status == OK)
            status = parse_number(line, base, &value);
        if (status == ERR_INPUT || status == ERR_NUMBER) {
            fprintf(stderr, "Invalid number skipped\n");
            continue;
        }
        if (status == OK)
            status = add_checked(*sum, value, sum);
        if (status != OK)
            return status;
        if (*count == 0 || magnitude_of(value) > magnitude_of(*max_value))
            *max_value = value;
        (*count)++;
    }
}

static status_t print_in_bases(const char *title, const ll value, const int input_base)
{
    const int bases[] = {input_base, 9, 18, 27, 36};
    char text[TEXT_SIZE];

    printf("%s\n", title);
    for (size_t i = 0; i < sizeof(bases) / sizeof(bases[0]); i++) {
        status_t status = to_base_text(value, bases[i], text, sizeof(text));

        if (status != OK)
            return status;
        printf("  base %2d: %s\n", bases[i], text);
    }
    return OK;
}

static const char *status_message(const status_t status)
{
    switch (status) {
    case ERR_INPUT: return "Input line is too long";
    case ERR_BASE: return "Base must be an integer in [2, 36]";
    case ERR_OVERFLOW: return "Value does not fit into long long";
    case END_OF_INPUT: return "Input ended unexpectedly";
    default: return "";
    }
}

int main(void)
{
    char line[LINE_SIZE];
    int base;
    ll max_value;
    ll sum;
    size_t count;
    status_t status;

    printf("Base [2..36]: ");
    status = read_line(line, sizeof(line));
    if (status == OK)
        status = parse_base(line, &base);
    if (status == OK)
        status = read_numbers(base, &max_value, &sum, &count);
    if (status == OK && count == 0) {
        printf("No numbers were entered\n");
        return 0;
    }
    if (status == OK)
        status = print_in_bases("Maximum by modulus:", max_value, base);
    if (status == OK)
        status = print_in_bases("Sum:", sum, base);
    if (status != OK) {
        fprintf(stderr, "%s\n", status_message(status));
        return 1;
    }
    return 0;
}
