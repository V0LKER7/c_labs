#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define MAX_MULTIPLE 100
#define MAX_EXPONENT_NUMBER 10
#define MAX_BASE_NUMBER 10
#define MAX_SUM_ARGUMENT 4294967295ULL
#define HEX_BUFFER_SIZE 32

typedef unsigned long long ull;

typedef enum {
    OK = 0,
    ERR_ARGS_COUNT,
    ERR_FLAG,
    ERR_NUMBER,
    ERR_RANGE,
    ERR_OVERFLOW
} status_t;

typedef enum {
    KIND_NEITHER,
    KIND_PRIME,
    KIND_COMPOSITE
} number_kind_t;

static status_t parse_flag(const char *arg, char *flag)
{
    if (arg == NULL || strlen(arg) != 2 || (arg[0] != '-' && arg[0] != '/'))
        return ERR_FLAG;
    *flag = arg[1];
    return OK;
}

static status_t parse_natural(const char *text, ull *number)
{
    char *end;

    if (text == NULL || text[0] < '0' || text[0] > '9')
        return ERR_NUMBER;
    errno = 0;
    *number = strtoull(text, &end, 10);
    if (errno == ERANGE)
        return ERR_OVERFLOW;
    if (*end != '\0')
        return ERR_NUMBER;
    return OK;
}

static void find_multiples(const ull x, unsigned *multiples, size_t *count)
{
    *count = 0;
    for (unsigned n = 1; n <= MAX_MULTIPLE; n++)
        if (n % x == 0)
            multiples[(*count)++] = n;
}

static number_kind_t classify_number(const ull x)
{
    if (x < 2)
        return KIND_NEITHER;
    for (ull divisor = 2; divisor <= x / divisor; divisor++)
        if (x % divisor == 0)
            return KIND_COMPOSITE;
    return KIND_PRIME;
}

static status_t to_hex(const ull x, char *buffer, const size_t size)
{
    int written = snprintf(buffer, size, "%llX", x);

    if (written < 0 || (size_t)written >= size)
        return ERR_OVERFLOW;
    return OK;
}

static ull power_of(const ull base, const ull exponent)
{
    ull result = 1;

    for (ull i = 0; i < exponent; i++)
        result *= base;
    return result;
}

static status_t sum_to(const ull x, ull *sum)
{
    if (x > MAX_SUM_ARGUMENT)
        return ERR_OVERFLOW;
    *sum = x * (x + 1) / 2;
    return OK;
}

static status_t factorial_of(const ull x, ull *result)
{
    *result = 1;
    for (ull i = 2; i <= x; i++) {
        if (*result > ~0ULL / i)
            return ERR_OVERFLOW;
        *result *= i;
    }
    return OK;
}

static status_t run_multiples(const ull x)
{
    unsigned multiples[MAX_MULTIPLE];
    size_t count;

    if (x == 0)
        return ERR_RANGE;
    find_multiples(x, multiples, &count);
    if (count == 0) {
        printf("Natural numbers up to %d divisible by %llu do not exist\n", MAX_MULTIPLE, x);
        return OK;
    }
    for (size_t i = 0; i < count; i++)
        printf("%u%c", multiples[i], i + 1 == count ? '\n' : ' ');
    return OK;
}

static status_t run_prime(const ull x)
{
    switch (classify_number(x)) {
    case KIND_PRIME:
        printf("%llu is prime\n", x);
        break;
    case KIND_COMPOSITE:
        printf("%llu is composite\n", x);
        break;
    default:
        printf("%llu is neither prime nor composite\n", x);
        break;
    }
    return OK;
}

static status_t run_split(const ull x)
{
    char buffer[HEX_BUFFER_SIZE];
    status_t status = to_hex(x, buffer, sizeof(buffer));

    if (status != OK)
        return status;
    for (size_t i = 0; buffer[i] != '\0'; i++)
        printf("%c%c", buffer[i], buffer[i + 1] == '\0' ? '\n' : ' ');
    return OK;
}

static status_t run_powers(const ull x)
{
    if (x < 1 || x > MAX_EXPONENT_NUMBER)
        return ERR_RANGE;
    for (ull base = 1; base <= MAX_BASE_NUMBER; base++) {
        printf("%2llu:", base);
        for (ull exponent = 1; exponent <= x; exponent++)
            printf(" %llu", power_of(base, exponent));
        printf("\n");
    }
    return OK;
}

static status_t run_sum(const ull x)
{
    ull sum;
    status_t status;

    if (x < 1)
        return ERR_RANGE;
    status = sum_to(x, &sum);
    if (status == OK)
        printf("%llu\n", sum);
    return status;
}

static status_t run_factorial(const ull x)
{
    ull result;
    status_t status = factorial_of(x, &result);

    if (status == OK)
        printf("%llu\n", result);
    return status;
}

static const char *status_message(const status_t status)
{
    switch (status) {
    case ERR_ARGS_COUNT: return "Usage: program <flag> <x>";
    case ERR_FLAG: return "Unknown flag";
    case ERR_NUMBER: return "x must be a non-negative integer";
    case ERR_RANGE: return "x is out of allowed range for this flag";
    case ERR_OVERFLOW: return "Result does not fit into the type";
    default: return "";
    }
}

int main(int argc, char *argv[])
{
    char flag;
    ull x;
    status_t status;

    if (argc != 3) {
        fprintf(stderr, "%s\n", status_message(ERR_ARGS_COUNT));
        return 1;
    }
    status = parse_flag(argv[1], &flag);
    if (status == OK)
        status = parse_natural(argv[2], &x);
    if (status == OK) {
        switch (flag) {
        case 'h': status = run_multiples(x); break;
        case 'p': status = run_prime(x); break;
        case 's': status = run_split(x); break;
        case 'e': status = run_powers(x); break;
        case 'a': status = run_sum(x); break;
        case 'f': status = run_factorial(x); break;
        default: status = ERR_FLAG; break;
        }
    }
    if (status != OK) {
        fprintf(stderr, "%s\n", status_message(status));
        return 1;
    }
    return 0;
}
