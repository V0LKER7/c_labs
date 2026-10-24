#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    STATUS_OK,
    STATUS_NULL_ARGUMENT,
    STATUS_NO_MEMORY,
    STATUS_INVALID_EPSILON,
    STATUS_INVALID_DEGREE
} status_t;

status_t shift_polynomial(double eps, double a, double **result, int degree, ...)
{
    if (result == NULL) {
        return STATUS_NULL_ARGUMENT;
    }
    if (eps < 0.0) {
        return STATUS_INVALID_EPSILON;
    }
    if (degree < 0) {
        return STATUS_INVALID_DEGREE;
    }
    size_t size = (size_t)degree + 1;
    double *coefficients = malloc(size * sizeof(double));
    if (coefficients == NULL) {
        return STATUS_NO_MEMORY;
    }
    va_list args;
    va_start(args, degree);
    for (size_t i = 0; i < size; i++) {
        coefficients[i] = va_arg(args, double);
    }
    va_end(args);
    double *shifted = malloc(size * sizeof(double));
    if (shifted == NULL) {
        free(coefficients);
        return STATUS_NO_MEMORY;
    }
    for (size_t i = 0; i < size; i++) {
        for (size_t j = size - 1; j > i; j--) {
            coefficients[j - 1] += a * coefficients[j];
        }
        shifted[i] = fabs(coefficients[i]) < eps ? 0.0 : coefficients[i];
    }
    free(coefficients);
    *result = shifted;
    return STATUS_OK;
}

static const char *status_message(status_t status)
{
    switch (status) {
    case STATUS_NULL_ARGUMENT:
        return "null argument";
    case STATUS_NO_MEMORY:
        return "out of memory";
    case STATUS_INVALID_EPSILON:
        return "epsilon must not be negative";
    case STATUS_INVALID_DEGREE:
        return "degree must not be negative";
    default:
        return "unknown error";
    }
}

static void print_coefficients(const char *title, const double *coefficients, int degree)
{
    printf("%s:", title);
    for (int i = 0; i <= degree; i++) {
        printf(" g%d=%g", i, coefficients[i]);
    }
    printf("\n");
}

int main(void)
{
    double *result = NULL;

    status_t status = shift_polynomial(1e-9, 1.0, &result, 2, 1.0, 2.0, 3.0);
    if (status != STATUS_OK) {
        fprintf(stderr, "Error: %s\n", status_message(status));
        return 1;
    }
    print_coefficients("1 + 2x + 3x^2 at a=1", result, 2);
    free(result);
    result = NULL;

    status = shift_polynomial(1e-9, 2.0, &result, 3, 1.0, -2.0, 0.0, 1.0);
    if (status != STATUS_OK) {
        fprintf(stderr, "Error: %s\n", status_message(status));
        return 1;
    }
    print_coefficients("1 - 2x + x^3 at a=2", result, 3);
    free(result);
    result = NULL;

    status = shift_polynomial(1e-9, 0.5, &result, 2, 0.0, 0.0, 1.0);
    if (status != STATUS_OK) {
        fprintf(stderr, "Error: %s\n", status_message(status));
        return 1;
    }
    print_coefficients("x^2 at a=0.5", result, 2);
    free(result);
    result = NULL;

    status = shift_polynomial(1e-9, 1.0, &result, 0, 5.0);
    if (status == STATUS_OK) {
        print_coefficients("constant 5 at a=1", result, 0);
        free(result);
    }
    status = shift_polynomial(1e-9, 1.0, &result, -1);
    printf("negative degree: %s\n", status_message(status));
    return 0;
}
