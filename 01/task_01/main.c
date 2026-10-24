#include <stdint.h>
#include <stdio.h>

#define MULTIPLES_LIMIT 100
#define POWER_TABLE_MAX_X 10
#define POWER_TABLE_MAX_BASE 10
#define HEX_DIGITS_MAX 16

typedef enum {
    STATUS_OK,
    STATUS_INVALID_ARGS_COUNT,
    STATUS_INVALID_FLAG,
    STATUS_INVALID_NUMBER,
    STATUS_OUT_OF_RANGE,
    STATUS_OVERFLOW
} status_t;

static const char *status_message(status_t status)
{
    switch (status) {
    case STATUS_OK:
        return "ok";
    case STATUS_INVALID_ARGS_COUNT:
        return "wrong number of arguments";
    case STATUS_INVALID_FLAG:
        return "unknown flag";
    case STATUS_INVALID_NUMBER:
        return "x must be a non-negative integer";
    case STATUS_OUT_OF_RANGE:
        return "x is out of range for this flag";
    case STATUS_OVERFLOW:
        return "result does not fit into 64 bits";
    }
    return "unknown error";
}

static status_t parse_uint64(const char *text, uint64_t *value)
{
    if (text == NULL || value == NULL || *text == '\0') {
        return STATUS_INVALID_NUMBER;
    }
    uint64_t result = 0;
    for (const char *symbol = text; *symbol != '\0'; symbol++) {
        if (*symbol < '0' || *symbol > '9') {
            return STATUS_INVALID_NUMBER;
        }
        uint64_t digit = (uint64_t)(*symbol - '0');
        if (result > (UINT64_MAX - digit) / 10) {
            return STATUS_OVERFLOW;
        }
        result = result * 10 + digit;
    }
    *value = result;
    return STATUS_OK;
}

static status_t parse_flag(const char *argument, char *flag)
{
    if (argument == NULL || flag == NULL) {
        return STATUS_INVALID_FLAG;
    }
    if ((argument[0] != '-' && argument[0] != '/') || argument[1] == '\0' ||
        argument[2] != '\0') {
        return STATUS_INVALID_FLAG;
    }
    *flag = argument[1];
    return STATUS_OK;
}

static int looks_like_flag(const char *argument)
{
    return (argument[0] == '-' || argument[0] == '/') && argument[1] != '\0' &&
           (argument[1] < '0' || argument[1] > '9');
}

static status_t find_multiples(uint64_t x, uint64_t *multiples, size_t *count)
{
    if (multiples == NULL || count == NULL) {
        return STATUS_INVALID_NUMBER;
    }
    if (x == 0) {
        return STATUS_OUT_OF_RANGE;
    }
    *count = 0;
    for (uint64_t current = x; current <= MULTIPLES_LIMIT; current += x) {
        multiples[(*count)++] = current;
    }
    return STATUS_OK;
}

static status_t classify_number(uint64_t x, int *is_prime, int *is_composite)
{
    if (is_prime == NULL || is_composite == NULL) {
        return STATUS_INVALID_NUMBER;
    }
    *is_prime = 0;
    *is_composite = 0;
    if (x < 2) {
        return STATUS_OK;
    }
    if (x % 2 == 0) {
        *is_prime = (x == 2);
        *is_composite = (x != 2);
        return STATUS_OK;
    }
    *is_prime = 1;
    for (uint64_t divisor = 3; divisor <= x / divisor; divisor += 2) {
        if (x % divisor == 0) {
            *is_prime = 0;
            *is_composite = 1;
            break;
        }
    }
    return STATUS_OK;
}

static status_t split_hex_digits(uint64_t x, char *digits, size_t *count)
{
    if (digits == NULL || count == NULL) {
        return STATUS_INVALID_NUMBER;
    }
    const char alphabet[] = "0123456789ABCDEF";
    char reversed[HEX_DIGITS_MAX];
    size_t length = 0;
    do {
        reversed[length++] = alphabet[x % 16];
        x /= 16;
    } while (x > 0);
    for (size_t index = 0; index < length; index++) {
        digits[index] = reversed[length - 1 - index];
    }
    *count = length;
    return STATUS_OK;
}

static status_t power_u64(uint64_t base, uint64_t exponent, uint64_t *result)
{
    if (result == NULL) {
        return STATUS_INVALID_NUMBER;
    }
    uint64_t value = 1;
    for (uint64_t step = 0; step < exponent; step++) {
        if (base != 0 && value > UINT64_MAX / base) {
            return STATUS_OVERFLOW;
        }
        value *= base;
    }
    *result = value;
    return STATUS_OK;
}

static status_t sum_to(uint64_t x, uint64_t *sum)
{
    if (sum == NULL) {
        return STATUS_INVALID_NUMBER;
    }
    if (x == UINT64_MAX) {
        return STATUS_OVERFLOW;
    }
    uint64_t first = x;
    uint64_t second = x + 1;
    if (first % 2 == 0) {
        first /= 2;
    } else {
        second /= 2;
    }
    if (second != 0 && first > UINT64_MAX / second) {
        return STATUS_OVERFLOW;
    }
    *sum = first * second;
    return STATUS_OK;
}

static status_t factorial(uint64_t x, uint64_t *result)
{
    if (result == NULL) {
        return STATUS_INVALID_NUMBER;
    }
    uint64_t value = 1;
    for (uint64_t factor = 2; factor <= x; factor++) {
        if (value > UINT64_MAX / factor) {
            return STATUS_OVERFLOW;
        }
        value *= factor;
    }
    *result = value;
    return STATUS_OK;
}

static status_t run_multiples(uint64_t x)
{
    uint64_t multiples[MULTIPLES_LIMIT];
    size_t count = 0;
    status_t status = find_multiples(x, multiples, &count);
    if (status != STATUS_OK) {
        return status;
    }
    if (count == 0) {
        printf("No natural numbers up to %d are multiples of %llu\n",
               MULTIPLES_LIMIT, (unsigned long long)x);
        return STATUS_OK;
    }
    for (size_t index = 0; index < count; index++) {
        printf("%llu%c", (unsigned long long)multiples[index],
               index + 1 == count ? '\n' : ' ');
    }
    return STATUS_OK;
}

static status_t run_prime(uint64_t x)
{
    int is_prime = 0;
    int is_composite = 0;
    status_t status = classify_number(x, &is_prime, &is_composite);
    if (status != STATUS_OK) {
        return status;
    }
    if (is_prime) {
        printf("%llu is prime\n", (unsigned long long)x);
    } else if (is_composite) {
        printf("%llu is composite\n", (unsigned long long)x);
    } else {
        printf("%llu is neither prime nor composite\n", (unsigned long long)x);
    }
    return STATUS_OK;
}

static status_t run_split(uint64_t x)
{
    char digits[HEX_DIGITS_MAX];
    size_t count = 0;
    status_t status = split_hex_digits(x, digits, &count);
    if (status != STATUS_OK) {
        return status;
    }
    for (size_t index = 0; index < count; index++) {
        printf("%c%c", digits[index], index + 1 == count ? '\n' : ' ');
    }
    return STATUS_OK;
}

static status_t run_power_table(uint64_t x)
{
    if (x < 1 || x > POWER_TABLE_MAX_X) {
        return STATUS_OUT_OF_RANGE;
    }
    for (uint64_t base = 1; base <= POWER_TABLE_MAX_BASE; base++) {
        printf("base %llu:", (unsigned long long)base);
        for (uint64_t exponent = 1; exponent <= x; exponent++) {
            uint64_t value = 0;
            status_t status = power_u64(base, exponent, &value);
            if (status != STATUS_OK) {
                return status;
            }
            printf(" %llu", (unsigned long long)value);
        }
        printf("\n");
    }
    return STATUS_OK;
}

static status_t run_sum(uint64_t x)
{
    uint64_t sum = 0;
    status_t status = sum_to(x, &sum);
    if (status != STATUS_OK) {
        return status;
    }
    printf("%llu\n", (unsigned long long)sum);
    return STATUS_OK;
}

static status_t run_factorial(uint64_t x)
{
    uint64_t result = 0;
    status_t status = factorial(x, &result);
    if (status != STATUS_OK) {
        return status;
    }
    printf("%llu\n", (unsigned long long)result);
    return STATUS_OK;
}

static status_t run_command(char flag, uint64_t x)
{
    switch (flag) {
    case 'h':
        return run_multiples(x);
    case 'p':
        return run_prime(x);
    case 's':
        return run_split(x);
    case 'e':
        return run_power_table(x);
    case 'a':
        return run_sum(x);
    case 'f':
        return run_factorial(x);
    default:
        return STATUS_INVALID_FLAG;
    }
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <x> <flag> or %s <flag> <x>, flags: -h -p -s -e -a -f\n", argv[0], argv[0]);
        return 1;
    }
    const char *number_argument = argv[1];
    const char *flag_argument = argv[2];
    if (looks_like_flag(argv[1])) {
        number_argument = argv[2];
        flag_argument = argv[1];
    }
    uint64_t x = 0;
    char flag = '\0';
    status_t status = parse_uint64(number_argument, &x);
    if (status == STATUS_OK) {
        status = parse_flag(flag_argument, &flag);
    }
    if (status == STATUS_OK) {
        status = run_command(flag, x);
    }
    if (status != STATUS_OK) {
        fprintf(stderr, "Error: %s\n", status_message(status));
        return 1;
    }
    return 0;
}
