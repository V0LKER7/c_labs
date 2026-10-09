#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    STATUS_OK,
    STATUS_NULL_POINTER,
    STATUS_NO_MEMORY,
    STATUS_INVALID_ARGUMENT
} Status;

Status shift_polynomial(double epsilon, double shift, double **result, int degree, ...)
{
    if (result == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (epsilon < 0.0 || degree < 0) {
        return STATUS_INVALID_ARGUMENT;
    }
    size_t size = (size_t)degree + 1;
    double *coefficients = malloc(size * sizeof *coefficients);
    if (coefficients == NULL) {
        return STATUS_NO_MEMORY;
    }
    va_list args;
    va_start(args, degree);
    for (size_t i = 0; i < size; i++) {
        coefficients[i] = va_arg(args, double);
    }
    va_end(args);

    for (size_t i = 0; i < size - 1; i++) {
        for (size_t j = size - 1; j > i; j--) {
            coefficients[j - 1] += shift * coefficients[j];
        }
    }
    for (size_t i = 0; i < size; i++) {
        if (fabs(coefficients[i]) < epsilon) {
            coefficients[i] = 0.0;
        }
    }
    *result = coefficients;
    return STATUS_OK;
}

static void print_coefficients(const char *title, Status status, const double *coefficients, int degree)
{
    printf("%s\n", title);
    if (status != STATUS_OK) {
        printf("  error %d\n", status);
        return;
    }
    for (int i = 0; i <= degree; i++) {
        printf("  g%d = %g\n", i, coefficients[i]);
    }
}

int main(void)
{
    double *result = NULL;

    Status status = shift_polynomial(1e-9, 1.0, &result, 2, 1.0, 2.0, 3.0);
    print_coefficients("f = 1 + 2x + 3x^2, a = 1", status, result, 2);
    free(result);

    status = shift_polynomial(1e-9, 2.0, &result, 3, 0.0, 0.0, 0.0, 1.0);
    print_coefficients("f = x^3, a = 2", status, result, 3);
    free(result);

    status = shift_polynomial(1e-9, -1.5, &result, 4, 5.0, -3.0, 0.0, 2.0, 1.0);
    print_coefficients("f = 5 - 3x + 2x^3 + x^4, a = -1.5", status, result, 4);
    free(result);

    status = shift_polynomial(1e-9, 3.0, &result, 0, 7.0);
    print_coefficients("f = 7, a = 3", status, result, 0);
    free(result);

    status = shift_polynomial(1e-9, 3.0, &result, -1);
    print_coefficients("degree -1", status, NULL, 0);
    return 0;
}
