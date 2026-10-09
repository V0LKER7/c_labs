#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>

#define MAX_DOUBLINGS 28
#define MAX_ITERATIONS 4000000000UL
#define GAMMA_MAX_M 50
#define GAMMA_MAX_ITERATIONS 2000000000UL
#define MAX_SIEVE_SIZE (1UL << 26)
#define MIN_EPS 1e-9L

typedef long double real;
typedef real (*function_t)(real);

typedef enum {
    OK = 0,
    ERR_ARGS_COUNT,
    ERR_EPS,
    ERR_NO_CONVERGENCE,
    ERR_INTERVAL,
    ERR_MEMORY
} status_t;

typedef struct {
    const char *constant;
    const char *method;
    status_t (*compute)(real, real *);
} entry_t;

static status_t parse_eps(const char *text, real *eps)
{
    char *end;
    double value;

    errno = 0;
    value = strtod(text, &end);
    if (errno != 0 || *end != '\0' || *text == '\0' || !isfinite(value))
        return ERR_EPS;
    if (value < MIN_EPS || value >= 1)
        return ERR_EPS;
    *eps = value;
    return OK;
}

static status_t limit_by_doubling(real (*term)(unsigned long), const real eps, real *result)
{
    unsigned long n = 1;
    real previous = term(n);

    for (int i = 0; i < MAX_DOUBLINGS; i++) {
        real current;

        n *= 2;
        current = term(n);
        if (fabsl(current - previous) < eps) {
            *result = current;
            return OK;
        }
        previous = current;
    }
    return ERR_NO_CONVERGENCE;
}

static status_t bisection(const function_t f, real left, real right, const real eps, real *root)
{
    real f_left = f(left);
    real f_right = f(right);

    if (f_left * f_right > 0)
        return ERR_INTERVAL;
    while (right - left >= eps) {
        real middle = (left + right) / 2;
        real f_middle = f(middle);

        if (f_left * f_middle <= 0) {
            right = middle;
        } else {
            left = middle;
            f_left = f_middle;
        }
    }
    *root = (left + right) / 2;
    return OK;
}

static real e_limit_term(const unsigned long n)
{
    return powl(1.0L + 1.0L / n, n);
}

static real pi_limit_term(const unsigned long n)
{
    real value = 4;

    for (unsigned long k = 2; k <= n; k++)
        value *= 4.0L * k * (k - 1) / ((2.0L * k - 1) * (2.0L * k - 1));
    return value;
}

static real ln2_limit_term(const unsigned long n)
{
    return n * (powl(2.0L, 1.0L / n) - 1);
}

static real e_equation(const real x)
{
    return logl(x) - 1;
}

static real ln2_equation(const real x)
{
    return expl(x) - 2;
}

static real sqrt2_equation(const real x)
{
    return x * x - 2;
}

static status_t e_limit(const real eps, real *result)
{
    return limit_by_doubling(e_limit_term, eps, result);
}

static status_t e_series(const real eps, real *result)
{
    real term = 1;

    *result = 1;
    for (unsigned long n = 1; n < MAX_ITERATIONS; n++) {
        term /= n;
        *result += term;
        if (term < eps)
            return OK;
    }
    return ERR_NO_CONVERGENCE;
}

static status_t e_solve(const real eps, real *result)
{
    return bisection(e_equation, 1, 4, eps, result);
}

static status_t pi_limit(const real eps, real *result)
{
    return limit_by_doubling(pi_limit_term, eps, result);
}

static status_t pi_series(const real eps, real *result)
{
    real sum = 0;

    for (unsigned long n = 1; n < MAX_ITERATIONS; n++) {
        real term = 1.0L / (2 * n - 1);

        sum += n % 2 == 1 ? term : -term;
        if (4 * term < eps) {
            *result = 4 * sum;
            return OK;
        }
    }
    return ERR_NO_CONVERGENCE;
}

static status_t pi_solve(const real eps, real *result)
{
    real left = 3;
    real right = 4;

    while (right - left >= eps) {
        real first = left + (right - left) / 3;
        real second = right - (right - left) / 3;

        if (cosl(first) < cosl(second))
            right = second;
        else
            left = first;
    }
    *result = (left + right) / 2;
    return OK;
}

static status_t ln2_limit(const real eps, real *result)
{
    return limit_by_doubling(ln2_limit_term, eps, result);
}

static status_t ln2_series(const real eps, real *result)
{
    *result = 0;
    for (unsigned long n = 1; n < MAX_ITERATIONS; n++) {
        real term = 1.0L / n;

        *result += n % 2 == 1 ? term : -term;
        if (term < eps)
            return OK;
    }
    return ERR_NO_CONVERGENCE;
}

static status_t ln2_solve(const real eps, real *result)
{
    return bisection(ln2_equation, 0, 1, eps, result);
}

static status_t sqrt2_limit(const real eps, real *result)
{
    real x = -0.5L;

    for (unsigned long n = 0; n < MAX_ITERATIONS; n++) {
        real next = x - x * x / 2 + 1;

        if (fabsl(next - x) < eps) {
            *result = next;
            return OK;
        }
        x = next;
    }
    return ERR_NO_CONVERGENCE;
}

static status_t sqrt2_product(const real eps, real *result)
{
    real product = 1;

    for (int k = 2; k < 64; k++) {
        real factor = exp2l(exp2l(-k));

        product *= factor;
        if (product * (factor - 1) < eps) {
            *result = product;
            return OK;
        }
    }
    return ERR_NO_CONVERGENCE;
}

static status_t sqrt2_solve(const real eps, real *result)
{
    return bisection(sqrt2_equation, 1, 2, eps, result);
}

static status_t gamma_limit(const real eps, real *result)
{
    real previous = 0;

    for (int m = 1; m <= GAMMA_MAX_M; m++) {
        real sum = 0;
        real binomial = 1;
        real log_factorial = 0;

        for (int k = 1; k <= m; k++) {
            binomial = binomial * (m - k + 1) / k;
            log_factorial += logl(k);
            sum += (k % 2 == 1 ? -1 : 1) * binomial / k * log_factorial;
        }
        if (m > 1 && m * fabsl(sum - previous) < eps) {
            *result = sum;
            return OK;
        }
        previous = sum;
    }
    return ERR_NO_CONVERGENCE;
}

static status_t gamma_series(const real eps, real *result)
{
    const real pi_squared_over_6 = 1.6449340668482264365L;
    real sum = -pi_squared_over_6;
    unsigned long root = 1;

    for (unsigned long k = 2; k < GAMMA_MAX_ITERATIONS; k++) {
        if ((root + 1) * (root + 1) == k)
            root++;
        sum += 1.0L / ((real)root * root) - 1.0L / k;
        if (k + 1 == (root + 1) * (root + 1) && 2.0L / root < eps) {
            *result = sum;
            return OK;
        }
    }
    return ERR_NO_CONVERGENCE;
}

static status_t mertens_value(const unsigned long limit, real *result)
{
    char *composite = calloc(limit + 1, 1);
    real product = 1;

    if (composite == NULL)
        return ERR_MEMORY;
    for (unsigned long i = 2; i <= limit; i++) {
        if (composite[i])
            continue;
        product *= (real)(i - 1) / i;
        for (unsigned long j = i * i; j <= limit; j += i)
            composite[j] = 1;
    }
    free(composite);
    *result = logl((real)limit) * product;
    return OK;
}

static status_t gamma_solve(const real eps, real *result)
{
    real previous;
    status_t status = mertens_value(1024, &previous);

    if (status != OK)
        return status;
    for (unsigned long limit = 2048; limit <= MAX_SIEVE_SIZE; limit *= 2) {
        real current;

        status = mertens_value(limit, &current);
        if (status != OK)
            return status;
        if (fabsl(logl(previous) - logl(current)) < eps) {
            *result = -logl(current);
            return OK;
        }
        previous = current;
    }
    return ERR_NO_CONVERGENCE;
}

static const char *status_message(const status_t status)
{
    switch (status) {
    case ERR_ARGS_COUNT: return "Usage: program <eps>";
    case ERR_EPS: return "eps must be a number in [1e-9, 1)";
    case ERR_NO_CONVERGENCE: return "no convergence within allowed limits";
    case ERR_INTERVAL: return "root is not bracketed by the interval";
    case ERR_MEMORY: return "memory allocation failed";
    default: return "";
    }
}

int main(int argc, char *argv[])
{
    static const entry_t entries[] = {
        {"e", "limit", e_limit},
        {"e", "series", e_series},
        {"e", "equation", e_solve},
        {"pi", "limit", pi_limit},
        {"pi", "series", pi_series},
        {"pi", "equation", pi_solve},
        {"ln2", "limit", ln2_limit},
        {"ln2", "series", ln2_series},
        {"ln2", "equation", ln2_solve},
        {"sqrt2", "limit", sqrt2_limit},
        {"sqrt2", "series", sqrt2_product},
        {"sqrt2", "equation", sqrt2_solve},
        {"gamma", "limit", gamma_limit},
        {"gamma", "series", gamma_series},
        {"gamma", "equation", gamma_solve}
    };
    real eps;
    status_t status;

    if (argc != 2) {
        fprintf(stderr, "%s\n", status_message(ERR_ARGS_COUNT));
        return 1;
    }
    status = parse_eps(argv[1], &eps);
    if (status != OK) {
        fprintf(stderr, "%s\n", status_message(status));
        return 1;
    }
    for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++) {
        real value;

        status = entries[i].compute(eps, &value);
        if (status == OK)
            printf("%-6s %-9s %.12Lf\n", entries[i].constant, entries[i].method, value);
        else
            printf("%-6s %-9s %s\n", entries[i].constant, entries[i].method, status_message(status));
    }
    return 0;
}
