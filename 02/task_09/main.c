#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_ITERATIONS 40
#define MAX_DENOMINATOR 1000000000000ULL

typedef enum {
    STATUS_OK,
    STATUS_NULL_POINTER,
    STATUS_NO_MEMORY,
    STATUS_INVALID_ARGUMENT,
    STATUS_NO_FRACTION
} Status;

static unsigned long long gcd(unsigned long long a, unsigned long long b)
{
    while (b != 0) {
        unsigned long long rest = a % b;
        a = b;
        b = rest;
    }
    return a;
}

static Status find_denominator(double value, double epsilon, unsigned long long *denominator)
{
    double rest = value;
    unsigned long long h_prev = 1, h_prev2 = 0;
    unsigned long long k_prev = 0, k_prev2 = 1;
    for (int i = 0; i < MAX_ITERATIONS; i++) {
        unsigned long long a = (unsigned long long)floor(rest);
        unsigned long long h = a * h_prev + h_prev2;
        unsigned long long k = a * k_prev + k_prev2;
        if (k > MAX_DENOMINATOR) {
            return STATUS_NO_FRACTION;
        }
        if (fabs(value - (double)h / (double)k) < epsilon) {
            *denominator = k;
            return STATUS_OK;
        }
        double fraction = rest - (double)a;
        if (fraction < 1e-15) {
            return STATUS_NO_FRACTION;
        }
        rest = 1.0 / fraction;
        h_prev2 = h_prev;
        h_prev = h;
        k_prev2 = k_prev;
        k_prev = k;
    }
    return STATUS_NO_FRACTION;
}

static int is_finite_in_base(unsigned long long denominator, int base)
{
    unsigned long long common;
    while ((common = gcd(denominator, (unsigned long long)base)) > 1) {
        denominator /= common;
    }
    return denominator == 1;
}

Status check_finite_fractions(double epsilon, int base, size_t count, int **results, ...)
{
    if (results == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (epsilon <= 0.0 || base < 2 || count == 0) {
        return STATUS_INVALID_ARGUMENT;
    }
    int *output = malloc(count * sizeof *output);
    if (output == NULL) {
        return STATUS_NO_MEMORY;
    }
    Status status = STATUS_OK;
    va_list args;
    va_start(args, results);
    for (size_t i = 0; i < count && status == STATUS_OK; i++) {
        double value = va_arg(args, double);
        unsigned long long denominator = 0;
        if (!(value > 0.0 && value < 1.0)) {
            status = STATUS_INVALID_ARGUMENT;
        } else {
            status = find_denominator(value, epsilon, &denominator);
        }
        if (status == STATUS_OK) {
            output[i] = is_finite_in_base(denominator, base);
        }
    }
    va_end(args);
    if (status != STATUS_OK) {
        free(output);
        return status;
    }
    *results = output;
    return STATUS_OK;
}

static void run(int base, size_t count, double first, double second, double third, double fourth)
{
    int *results = NULL;
    Status status = check_finite_fractions(1e-9, base, count, &results, first, second, third, fourth);
    if (status != STATUS_OK) {
        printf("base %d: error %d\n", base, status);
        return;
    }
    const double values[] = {first, second, third, fourth};
    printf("base %d:\n", base);
    for (size_t i = 0; i < count; i++) {
        printf("  %.6f -> %s\n", values[i], results[i] ? "finite" : "infinite");
    }
    free(results);
}

int main(void)
{
    run(2, 4, 0.5, 0.1, 0.25, 0.75);
    run(10, 4, 0.1, 0.125, 1.0 / 3.0, 0.2);
    run(3, 4, 1.0 / 3.0, 0.5, 1.0 / 9.0, 0.2);
    run(6, 4, 1.0 / 3.0, 0.5, 0.1, 1.0 / 6.0);
    run(10, 2, 1.5, 0.5, 0.5, 0.5);
    return 0;
}
