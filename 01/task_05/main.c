#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_SERIES_STEPS 10000000UL
#define MAX_INTEGRAL_INTERVALS (1UL << 26)

typedef enum {
    STATUS_OK,
    STATUS_INVALID_ARGS_COUNT,
    STATUS_INVALID_NUMBER,
    STATUS_INVALID_EPS,
    STATUS_INVALID_X,
    STATUS_NO_CONVERGENCE
} status_t;

typedef double (*ratio_func_t)(unsigned long n, double x);
typedef double (*integrand_func_t)(double x);

static const char *status_message(status_t status)
{
    switch (status) {
    case STATUS_OK:
        return "ok";
    case STATUS_INVALID_ARGS_COUNT:
        return "wrong number of arguments";
    case STATUS_INVALID_NUMBER:
        return "argument is not a valid number";
    case STATUS_INVALID_EPS:
        return "epsilon must be in (0, 1)";
    case STATUS_INVALID_X:
        return "x must satisfy |x| < 1";
    case STATUS_NO_CONVERGENCE:
        return "no convergence for this epsilon";
    }
    return "unknown error";
}

static status_t parse_double(const char *text, double *value)
{
    if (text == NULL || value == NULL || *text == '\0') {
        return STATUS_INVALID_NUMBER;
    }
    char *end = NULL;
    errno = 0;
    double result = strtod(text, &end);
    if (errno == ERANGE || *end != '\0' || !isfinite(result)) {
        return STATUS_INVALID_NUMBER;
    }
    *value = result;
    return STATUS_OK;
}

static double ratio_exp(unsigned long n, double x)
{
    return x / (double)n;
}

static double ratio_cos(unsigned long n, double x)
{
    return -x * x / ((2.0 * (double)n - 1.0) * (2.0 * (double)n));
}

static double ratio_triple(unsigned long n, double x)
{
    double k = (double)n;
    return 9.0 * k * k * x * x / ((3.0 * k - 1.0) * (3.0 * k - 2.0));
}

static double ratio_double_factorial(unsigned long n, double x)
{
    return -(2.0 * (double)n - 1.0) / (2.0 * (double)n) * x * x;
}

static status_t sum_series(ratio_func_t ratio, double x, double eps,
                           int include_first_term, double *result)
{
    if (ratio == NULL || result == NULL || eps <= 0.0) {
        return STATUS_INVALID_EPS;
    }
    double term = 1.0;
    double sum = include_first_term ? 1.0 : 0.0;
    for (unsigned long n = 1; n < MAX_SERIES_STEPS; n++) {
        term *= ratio(n, x);
        sum += term;
        if (fabs(term) < eps && fabs(ratio(n + 1, x)) < 1.0) {
            *result = sum;
            return STATUS_OK;
        }
    }
    return STATUS_NO_CONVERGENCE;
}

static double integrand_log_ratio(double x)
{
    return log1p(x) / x;
}

static double integrand_gauss(double x)
{
    return exp(-x * x / 2.0);
}

static double integrand_log_inverse(double x)
{
    return -log1p(-x);
}

static double integrand_power_self(double x)
{
    return pow(x, x);
}

static double midpoint_sum(integrand_func_t func, unsigned long intervals)
{
    double step = 1.0 / (double)intervals;
    double sum = 0.0;
    for (unsigned long index = 0; index < intervals; index++) {
        sum += func((double)index * step + step / 2.0);
    }
    return sum * step;
}

static status_t integrate_unit(integrand_func_t func, double eps, double *result)
{
    if (func == NULL || result == NULL || eps <= 0.0) {
        return STATUS_INVALID_EPS;
    }
    unsigned long intervals = 1;
    double previous = midpoint_sum(func, intervals);
    while (intervals < MAX_INTEGRAL_INTERVALS) {
        intervals *= 2;
        double current = midpoint_sum(func, intervals);
        if (fabs(current - previous) < eps) {
            *result = current;
            return STATUS_OK;
        }
        previous = current;
    }
    return STATUS_NO_CONVERGENCE;
}

static void print_value(const char *name, status_t status, double value)
{
    if (status == STATUS_OK) {
        printf("%-4s %.10f\n", name, value);
    } else {
        printf("%-4s %s\n", name, status_message(status));
    }
}

static void print_sums(double x, double eps)
{
    double value = 0.0;
    status_t status = sum_series(ratio_exp, x, eps, 1, &value);
    print_value("5.1a", status, value);
    status = sum_series(ratio_cos, x, eps, 1, &value);
    print_value("5.1b", status, value);
    status = sum_series(ratio_triple, x, eps, 1, &value);
    print_value("5.1c", status, value);
    status = sum_series(ratio_double_factorial, x, eps, 0, &value);
    print_value("5.1d", status, value);
}

static void print_integrals(double eps)
{
    double value = 0.0;
    status_t status = integrate_unit(integrand_log_ratio, eps, &value);
    print_value("5.2a", status, value);
    status = integrate_unit(integrand_gauss, eps, &value);
    print_value("5.2b", status, value);
    status = integrate_unit(integrand_log_inverse, eps, &value);
    print_value("5.2c", status, value);
    status = integrate_unit(integrand_power_self, eps, &value);
    print_value("5.2d", status, value);
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <epsilon> <x>\n", argv[0]);
        return 1;
    }
    double eps = 0.0;
    double x = 0.0;
    status_t status = parse_double(argv[1], &eps);
    if (status == STATUS_OK && (eps <= 0.0 || eps >= 1.0)) {
        status = STATUS_INVALID_EPS;
    }
    if (status == STATUS_OK) {
        status = parse_double(argv[2], &x);
    }
    if (status == STATUS_OK && fabs(x) >= 1.0) {
        status = STATUS_INVALID_X;
    }
    if (status != STATUS_OK) {
        fprintf(stderr, "Error: %s\n", status_message(status));
        return 1;
    }
    print_sums(x, eps);
    print_integrals(eps);
    return 0;
}
