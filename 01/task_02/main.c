#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_SERIES_STEPS 200000000UL
#define MAX_RECURRENCE_STEPS 1000000UL
#define MAX_GAMMA_LIMIT_M 30
#define MAX_PRIMES_BOUND 50000000UL
#define START_PRIMES_BOUND 1000UL
#define FACTORIAL_LOG_TABLE_SIZE (MAX_GAMMA_LIMIT_M + 1)

typedef enum {
    STATUS_OK,
    STATUS_INVALID_ARGS_COUNT,
    STATUS_INVALID_EPS,
    STATUS_INVALID_INTERVAL,
    STATUS_NO_CONVERGENCE,
    STATUS_MEMORY_ERROR
} status_t;

typedef double (*sequence_func_t)(unsigned long long n);
typedef double (*equation_func_t)(double x, double param);

static const char *status_message(status_t status)
{
    switch (status) {
    case STATUS_OK:
        return "ok";
    case STATUS_INVALID_ARGS_COUNT:
        return "wrong number of arguments";
    case STATUS_INVALID_EPS:
        return "epsilon must be a real number in (0, 1)";
    case STATUS_INVALID_INTERVAL:
        return "invalid interval for bisection";
    case STATUS_NO_CONVERGENCE:
        return "no convergence for this epsilon";
    case STATUS_MEMORY_ERROR:
        return "memory allocation failed";
    }
    return "unknown error";
}

static status_t parse_eps(const char *text, double *eps)
{
    if (text == NULL || eps == NULL || *text == '\0') {
        return STATUS_INVALID_EPS;
    }
    char *end = NULL;
    errno = 0;
    double value = strtod(text, &end);
    if (errno == ERANGE || *end != '\0' || !isfinite(value) || value <= 0.0 ||
        value >= 1.0) {
        return STATUS_INVALID_EPS;
    }
    *eps = value;
    return STATUS_OK;
}

static double pi_value(void)
{
    return acos(-1.0);
}

static status_t limit_by_doubling(sequence_func_t sequence, double eps,
                                  unsigned long long max_n, double *result)
{
    if (sequence == NULL || result == NULL || eps <= 0.0) {
        return STATUS_INVALID_EPS;
    }
    unsigned long long n = 1;
    double previous = sequence(n);
    while (n <= max_n) {
        n *= 2;
        double current = sequence(n);
        if (fabs(current - previous) < eps) {
            *result = current;
            return STATUS_OK;
        }
        previous = current;
    }
    return STATUS_NO_CONVERGENCE;
}

static status_t solve_bisection(equation_func_t func, double param, double left,
                                double right, double eps, double *root)
{
    if (func == NULL || root == NULL || eps <= 0.0 || left >= right) {
        return STATUS_INVALID_INTERVAL;
    }
    double value_left = func(left, param);
    double value_right = func(right, param);
    if ((value_left > 0.0) == (value_right > 0.0) && value_left != 0.0 &&
        value_right != 0.0) {
        return STATUS_INVALID_INTERVAL;
    }
    while (right - left > eps) {
        double middle = left + (right - left) / 2.0;
        if (middle <= left || middle >= right) {
            break;
        }
        if ((func(middle, param) > 0.0) == (value_left > 0.0)) {
            left = middle;
        } else {
            right = middle;
        }
    }
    *root = left + (right - left) / 2.0;
    return STATUS_OK;
}

static double e_sequence(unsigned long long n)
{
    return pow(1.0 + 1.0 / (double)n, (double)n);
}

static double pi_sequence(unsigned long long n)
{
    double ratio = 1.0;
    for (unsigned long long k = 1; k <= n; k++) {
        ratio *= 2.0 * (double)k / (2.0 * (double)k - 1.0);
    }
    return ratio * ratio / (double)n;
}

static double ln2_sequence(unsigned long long n)
{
    return (double)n * expm1(log(2.0) / (double)n);
}

static double e_equation(double x, double param)
{
    (void)param;
    return log(x) - 1.0;
}

static double pi_equation(double x, double param)
{
    (void)param;
    return cos(x / 2.0);
}

static double ln2_equation(double x, double param)
{
    (void)param;
    return exp(x) - 2.0;
}

static double sqrt2_equation(double x, double param)
{
    (void)param;
    return x * x - 2.0;
}

static double gamma_equation(double x, double param)
{
    return exp(-x) - param;
}

static status_t e_series(double eps, double *result)
{
    double term = 1.0;
    double sum = 1.0;
    for (unsigned long n = 1; n < MAX_SERIES_STEPS; n++) {
        term /= (double)n;
        sum += term;
        if (term < eps) {
            *result = sum;
            return STATUS_OK;
        }
    }
    return STATUS_NO_CONVERGENCE;
}

static status_t pi_series(double eps, double *result)
{
    double sum = 0.0;
    double sign = 1.0;
    for (unsigned long n = 1; n < MAX_SERIES_STEPS; n++) {
        double term = 4.0 / (2.0 * (double)n - 1.0);
        sum += sign * term;
        sign = -sign;
        if (term < eps) {
            *result = sum;
            return STATUS_OK;
        }
    }
    return STATUS_NO_CONVERGENCE;
}

static status_t ln2_series(double eps, double *result)
{
    double sum = 0.0;
    double sign = 1.0;
    for (unsigned long n = 1; n < MAX_SERIES_STEPS; n++) {
        double term = 1.0 / (double)n;
        sum += sign * term;
        sign = -sign;
        if (term < eps) {
            *result = sum;
            return STATUS_OK;
        }
    }
    return STATUS_NO_CONVERGENCE;
}

static status_t sqrt2_product(double eps, double *result)
{
    double product = 1.0;
    for (int k = 2; k < 64; k++) {
        double factor = pow(2.0, ldexp(1.0, -k));
        product *= factor;
        if (factor - 1.0 < eps) {
            *result = product;
            return STATUS_OK;
        }
    }
    *result = product;
    return STATUS_OK;
}

static status_t sqrt2_limit(double eps, double *result)
{
    double current = -0.5;
    for (unsigned long step = 0; step < MAX_RECURRENCE_STEPS; step++) {
        double next = current - current * current / 2.0 + 1.0;
        if (fabs(next - current) < eps) {
            *result = next;
            return STATUS_OK;
        }
        current = next;
    }
    return STATUS_NO_CONVERGENCE;
}

static double gamma_tail_estimate(double blocks)
{
    return 2.0 / blocks + 1.0 / (6.0 * blocks * blocks);
}

static status_t gamma_series(double eps, double *result)
{
    double sum = 0.0;
    unsigned long long block = 1;
    while (1.0 / ((double)block * (double)block * (double)block) >= eps) {
        double square = (double)(block * block);
        unsigned long long end = (block + 1) * (block + 1);
        for (unsigned long long k = block * block; k < end; k++) {
            sum += 1.0 / square - 1.0 / (double)k;
        }
        block++;
    }
    *result = -pi_value() * pi_value() / 6.0 + sum +
              gamma_tail_estimate((double)block);
    return STATUS_OK;
}

static long double gamma_limit_step(int m)
{
    long double log_factorial[FACTORIAL_LOG_TABLE_SIZE];
    log_factorial[0] = 0.0L;
    for (int i = 1; i <= m; i++) {
        log_factorial[i] = log_factorial[i - 1] + logl((long double)i);
    }
    long double binomial = 1.0L;
    long double sum = 0.0L;
    for (int k = 1; k <= m; k++) {
        binomial = binomial * (long double)(m - k + 1) / (long double)k;
        long double term = binomial / (long double)k * log_factorial[k];
        sum += (k % 2 == 0) ? term : -term;
    }
    return sum;
}

static status_t gamma_limit(double eps, double *result)
{
    long double previous = gamma_limit_step(1);
    for (int m = 2; m <= MAX_GAMMA_LIMIT_M; m++) {
        long double current = gamma_limit_step(m);
        if (fabsl(current - previous) * (long double)m < (long double)eps) {
            *result = (double)current;
            return STATUS_OK;
        }
        previous = current;
    }
    return STATUS_NO_CONVERGENCE;
}

static status_t mertens_value(unsigned long bound, double *value)
{
    unsigned char *composite = calloc(bound + 1, sizeof(unsigned char));
    if (composite == NULL) {
        return STATUS_MEMORY_ERROR;
    }
    double product = 1.0;
    for (unsigned long p = 2; p <= bound; p++) {
        if (composite[p]) {
            continue;
        }
        product *= (double)(p - 1) / (double)p;
        if (p <= bound / p) {
            for (unsigned long multiple = p * p; multiple <= bound;
                 multiple += p) {
                composite[multiple] = 1;
            }
        }
    }
    free(composite);
    *value = log((double)bound) * product;
    return STATUS_OK;
}

static status_t gamma_equation_solution(double eps, double *result)
{
    double previous = 0.0;
    int has_previous = 0;
    for (unsigned long bound = START_PRIMES_BOUND; bound <= MAX_PRIMES_BOUND;
         bound *= 2) {
        double limit_value = 0.0;
        status_t status = mertens_value(bound, &limit_value);
        if (status != STATUS_OK) {
            return status;
        }
        double current = 0.0;
        status = solve_bisection(gamma_equation, limit_value, 0.0, 1.0, eps / 10.0,
                                 &current);
        if (status != STATUS_OK) {
            return status;
        }
        if (has_previous && fabs(current - previous) < eps) {
            *result = current;
            return STATUS_OK;
        }
        previous = current;
        has_previous = 1;
    }
    return STATUS_NO_CONVERGENCE;
}

static status_t equation_constant(equation_func_t func, double left, double right,
                                  double eps, double *result)
{
    return solve_bisection(func, 0.0, left, right, eps, result);
}

static void print_result(const char *constant, const char *method,
                         status_t status, double value)
{
    if (status == STATUS_OK) {
        printf("%-6s %-9s %.12f\n", constant, method, value);
    } else {
        printf("%-6s %-9s %s\n", constant, method, status_message(status));
    }
}

static void demonstrate(double eps)
{
    double value = 0.0;
    status_t status;

    status = limit_by_doubling(e_sequence, eps, 1UL << 30, &value);
    print_result("e", "limit", status, value);
    status = e_series(eps, &value);
    print_result("e", "series", status, value);
    status = equation_constant(e_equation, 1.0, 3.0, eps, &value);
    print_result("e", "equation", status, value);

    status = limit_by_doubling(pi_sequence, eps, 1UL << 26, &value);
    print_result("pi", "limit", status, value);
    status = pi_series(eps, &value);
    print_result("pi", "series", status, value);
    status = equation_constant(pi_equation, 3.0, 4.0, eps, &value);
    print_result("pi", "equation", status, value);

    status = limit_by_doubling(ln2_sequence, eps, 1UL << 30, &value);
    print_result("ln2", "limit", status, value);
    status = ln2_series(eps, &value);
    print_result("ln2", "series", status, value);
    status = equation_constant(ln2_equation, 0.0, 1.0, eps, &value);
    print_result("ln2", "equation", status, value);

    status = sqrt2_limit(eps, &value);
    print_result("sqrt2", "limit", status, value);
    status = sqrt2_product(eps, &value);
    print_result("sqrt2", "product", status, value);
    status = equation_constant(sqrt2_equation, 1.0, 2.0, eps, &value);
    print_result("sqrt2", "equation", status, value);

    status = gamma_limit(eps, &value);
    print_result("gamma", "limit", status, value);
    status = gamma_series(eps, &value);
    print_result("gamma", "series", status, value);
    status = gamma_equation_solution(eps, &value);
    print_result("gamma", "equation", status, value);
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <epsilon>\n", argv[0]);
        return 1;
    }
    double eps = 0.0;
    status_t status = parse_eps(argv[1], &eps);
    if (status != STATUS_OK) {
        fprintf(stderr, "Error: %s\n", status_message(status));
        return 1;
    }
    demonstrate(eps);
    return 0;
}
