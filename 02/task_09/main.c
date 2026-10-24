#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_DENOMINATOR 1000000000000ULL
#define MAX_ITERATIONS 64

typedef enum {
    STATUS_OK,
    STATUS_NULL_ARGUMENT,
    STATUS_NO_MEMORY,
    STATUS_INVALID_BASE,
    STATUS_INVALID_EPSILON,
    STATUS_INVALID_NUMBER,
    STATUS_PRECISION_LOST
} status_t;

static unsigned long long gcd(unsigned long long a, unsigned long long b)
{
    while (b != 0) {
        unsigned long long rest = a % b;
        a = b;
        b = rest;
    }
    return a;
}

static status_t find_denominator(double value, double eps, unsigned long long *denominator)
{
    unsigned long long prev_num = 0;
    unsigned long long prev_den = 1;
    unsigned long long curr_num = 1;
    unsigned long long curr_den = 0;
    double rest = value;
    for (int i = 0; i < MAX_ITERATIONS; i++) {
        double whole = floor(rest);
        if (whole > (double)MAX_DENOMINATOR) {
            return STATUS_PRECISION_LOST;
        }
        unsigned long long part = (unsigned long long)whole;
        if (curr_den != 0 && part > (MAX_DENOMINATOR - prev_den) / curr_den) {
            return STATUS_PRECISION_LOST;
        }
        unsigned long long next_num = part * curr_num + prev_num;
        unsigned long long next_den = part * curr_den + prev_den;
        prev_num = curr_num;
        prev_den = curr_den;
        curr_num = next_num;
        curr_den = next_den;
        if (fabs(value - (double)curr_num / (double)curr_den) <= eps) {
            *denominator = curr_den;
            return STATUS_OK;
        }
        double fraction = rest - whole;
        if (fraction <= 0.0) {
            *denominator = curr_den;
            return STATUS_OK;
        }
        rest = 1.0 / fraction;
    }
    return STATUS_PRECISION_LOST;
}

static int has_finite_representation(unsigned long long denominator, int base)
{
    unsigned long long divisor = gcd(denominator, (unsigned long long)base);
    while (divisor > 1) {
        denominator /= divisor;
        divisor = gcd(denominator, (unsigned long long)base);
    }
    return denominator == 1;
}

status_t check_finite_representation(double eps, int base, size_t count, int **answers, ...)
{
    if (answers == NULL) {
        return STATUS_NULL_ARGUMENT;
    }
    if (base < 2) {
        return STATUS_INVALID_BASE;
    }
    if (!(eps > 0.0 && eps < 1.0)) {
        return STATUS_INVALID_EPSILON;
    }
    int *output = malloc((count == 0 ? 1 : count) * sizeof(int));
    if (output == NULL) {
        return STATUS_NO_MEMORY;
    }
    status_t status = STATUS_OK;
    va_list args;
    va_start(args, answers);
    for (size_t i = 0; i < count && status == STATUS_OK; i++) {
        double value = va_arg(args, double);
        unsigned long long denominator = 0;
        if (!(value > eps && value < 1.0 - eps)) {
            status = STATUS_INVALID_NUMBER;
        } else {
            status = find_denominator(value, eps, &denominator);
        }
        if (status == STATUS_OK) {
            output[i] = has_finite_representation(denominator, base);
        }
    }
    va_end(args);
    if (status != STATUS_OK) {
        free(output);
        return status;
    }
    *answers = output;
    return STATUS_OK;
}

static const char *status_message(status_t status)
{
    switch (status) {
    case STATUS_NULL_ARGUMENT:
        return "null argument";
    case STATUS_NO_MEMORY:
        return "out of memory";
    case STATUS_INVALID_BASE:
        return "base must be at least 2";
    case STATUS_INVALID_EPSILON:
        return "epsilon must be in (0; 1)";
    case STATUS_INVALID_NUMBER:
        return "number must be in (0; 1)";
    case STATUS_PRECISION_LOST:
        return "cannot find fraction with given epsilon";
    default:
        return "unknown error";
    }
}

int main(void)
{
    int *answers = NULL;
    const double values[] = {0.5, 0.1, 0.25, 0.333333333333, 0.125, 0.2, 0.375, 0.123456};
    const int bases[] = {2, 3, 10, 12};
    for (size_t b = 0; b < 4; b++) {
        status_t status = check_finite_representation(1e-9, bases[b], 8, &answers, values[0], values[1],
                                                      values[2], values[3], values[4], values[5],
                                                      values[6], values[7]);
        if (status != STATUS_OK) {
            fprintf(stderr, "Error: %s\n", status_message(status));
            return 1;
        }
        printf("base %d:\n", bases[b]);
        for (size_t i = 0; i < 8; i++) {
            printf("  %.12g -> %s\n", values[i], answers[i] ? "finite" : "infinite");
        }
        free(answers);
        answers = NULL;
    }
    printf("invalid number: %s\n",
           status_message(check_finite_representation(1e-9, 10, 1, &answers, 1.5)));
    printf("invalid base: %s\n",
           status_message(check_finite_representation(1e-9, 1, 1, &answers, 0.5)));
    return 0;
}
