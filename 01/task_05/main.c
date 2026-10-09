#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>

#define MAX_TERMS 10000000UL
#define MAX_INTERVALS (1UL << 26)
#define MIN_EPS 1e-12

typedef double (*function_t)(double);

typedef enum {
    OK = 0,
    ERR_ARGS_COUNT,
    ERR_NUMBER,
    ERR_EPS,
    ERR_DIVERGES,
    ERR_NO_CONVERGENCE
} status_t;

static status_t parse_double(const char *text, double *number)
{
    char *end;

    if (text == NULL || *text == '\0')
        return ERR_NUMBER;
    errno = 0;
    *number = strtod(text, &end);
    if (errno != 0 || *end != '\0' || !isfinite(*number))
        return ERR_NUMBER;
    return OK;
}

static status_t parse_eps(const char *text, double *eps)
{
    if (parse_double(text, eps) != OK || *eps < MIN_EPS || *eps >= 1)
        return ERR_EPS;
    return OK;
}

static status_t sum_exponent(const double x, const double eps, double *result)
{
    double term = 1;

    *result = 1;
    for (unsigned long n = 1; n < MAX_TERMS; n++) {
        term *= x / n;
        *result += term;
        if (!isfinite(term))
            return ERR_NO_CONVERGENCE;
        if (fabs(term) < eps)
            return OK;
    }
    return ERR_NO_CONVERGENCE;
}

static status_t sum_cosine(const double x, const double eps, double *result)
{
    double term = 1;

    *result = 1;
    for (unsigned long n = 1; n < MAX_TERMS; n++) {
        term *= -x * x / ((2.0 * n - 1) * (2.0 * n));
        *result += term;
        if (!isfinite(term))
            return ERR_NO_CONVERGENCE;
        if (fabs(term) < eps)
            return OK;
    }
    return ERR_NO_CONVERGENCE;
}

static status_t sum_factorial_ratio(const double x, const double eps, double *result)
{
    double term = 1;

    if (fabs(x) >= 1)
        return ERR_DIVERGES;
    *result = 1;
    for (unsigned long n = 1; n < MAX_TERMS; n++) {
        term *= 9.0 * n * n * x * x / ((3.0 * n - 1) * (3.0 * n - 2));
        *result += term;
        if (fabs(term) < eps)
            return OK;
    }
    return ERR_NO_CONVERGENCE;
}

static status_t sum_double_factorial(const double x, const double eps, double *result)
{
    double term = 1;

    if (fabs(x) >= 1)
        return ERR_DIVERGES;
    *result = 0;
    for (unsigned long n = 1; n < MAX_TERMS; n++) {
        term *= -(2.0 * n - 1) * x * x / (2.0 * n);
        *result += term;
        if (fabs(term) < eps)
            return OK;
    }
    return ERR_NO_CONVERGENCE;
}

static double function_log(const double x)
{
    return log(1 + x) / x;
}

static double function_gauss(const double x)
{
    return exp(-x * x / 2);
}

static double function_log_inverse(const double x)
{
    return log(1 / (1 - x));
}

static double function_power(const double x)
{
    return pow(x, x);
}

static double midpoint_rule(const function_t f, const unsigned long intervals)
{
    double step = 1.0 / intervals;
    double sum = 0;

    for (unsigned long i = 0; i < intervals; i++)
        sum += f((i + 0.5) * step);
    return sum * step;
}

static status_t integrate(const function_t f, const double eps, double *result)
{
    unsigned long intervals = 1;
    double previous = midpoint_rule(f, intervals);

    while (intervals < MAX_INTERVALS) {
        double current;

        intervals *= 2;
        current = midpoint_rule(f, intervals);
        if (fabs(current - previous) < eps) {
            *result = current;
            return OK;
        }
        previous = current;
    }
    return ERR_NO_CONVERGENCE;
}

static void print_result(const char *name, const status_t status, const double value)
{
    switch (status) {
    case OK:
        printf("%-4s %.10f\n", name, value);
        break;
    case ERR_DIVERGES:
        printf("%-4s series diverges for this x\n", name);
        break;
    default:
        printf("%-4s no convergence\n", name);
        break;
    }
}

int main(int argc, char *argv[])
{
    static const struct {
        const char *name;
        status_t (*sum)(double, double, double *);
    } sums[] = {
        {"1a", sum_exponent},
        {"1b", sum_cosine},
        {"1c", sum_factorial_ratio},
        {"1d", sum_double_factorial}
    };
    static const struct {
        const char *name;
        function_t function;
    } integrals[] = {
        {"2a", function_log},
        {"2b", function_gauss},
        {"2c", function_log_inverse},
        {"2d", function_power}
    };
    double eps;
    double x;

    if (argc != 3) {
        fprintf(stderr, "Usage: program <eps> <x>\n");
        return 1;
    }
    if (parse_eps(argv[1], &eps) != OK) {
        fprintf(stderr, "eps must be a number in [1e-12, 1)\n");
        return 1;
    }
    if (parse_double(argv[2], &x) != OK) {
        fprintf(stderr, "x must be a number\n");
        return 1;
    }
    for (size_t i = 0; i < sizeof(sums) / sizeof(sums[0]); i++) {
        double value = 0;
        status_t status = sums[i].sum(x, eps, &value);

        print_result(sums[i].name, status, value);
    }
    for (size_t i = 0; i < sizeof(integrals) / sizeof(integrals[0]); i++) {
        double value = 0;
        status_t status = integrate(integrals[i].function, eps, &value);

        print_result(integrals[i].name, status, value);
    }
    return 0;
}
