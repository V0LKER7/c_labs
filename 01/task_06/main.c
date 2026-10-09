#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#define PI 3.14159265358979323846
#define MAX_ROOT_ITERATIONS 10000

typedef unsigned long long ull;
typedef double (*function_t)(double);

typedef enum {
    OK = 0,
    ERR_ARGUMENT,
    ERR_MEMORY,
    ERR_DIGIT,
    ERR_OVERFLOW,
    ERR_DOMAIN,
    ERR_INTERVAL
} status_t;

static status_t is_convex_polygon(int *result, const double eps, const int points_count, ...)
{
    va_list args;
    double *x;
    double *y;
    double total_turn = 0;
    int sign = 0;

    if (result == NULL || eps <= 0 || points_count < 3)
        return ERR_ARGUMENT;
    x = malloc(points_count * sizeof(double));
    y = malloc(points_count * sizeof(double));
    if (x == NULL || y == NULL) {
        free(x);
        free(y);
        return ERR_MEMORY;
    }
    va_start(args, points_count);
    for (int i = 0; i < points_count; i++) {
        x[i] = va_arg(args, double);
        y[i] = va_arg(args, double);
    }
    va_end(args);
    *result = 1;
    for (int i = 0; i < points_count && *result; i++) {
        int j = (i + 1) % points_count;
        int k = (i + 2) % points_count;
        double ax = x[j] - x[i];
        double ay = y[j] - y[i];
        double bx = x[k] - x[j];
        double by = y[k] - y[j];
        double cross = ax * by - ay * bx;
        int current_sign = cross > eps ? 1 : (cross < -eps ? -1 : 0);

        if (current_sign != 0) {
            if (sign != 0 && current_sign != sign)
                *result = 0;
            sign = current_sign;
        }
        total_turn += atan2(cross, ax * bx + ay * by);
    }
    if (sign == 0 || fabs(fabs(total_turn) - 2 * PI) > eps)
        *result = 0;
    free(x);
    free(y);
    return OK;
}

static status_t evaluate_polynomial(double *result, const double x, const int degree, ...)
{
    va_list args;

    if (result == NULL || degree < 0)
        return ERR_ARGUMENT;
    va_start(args, degree);
    *result = va_arg(args, double);
    for (int i = 1; i <= degree; i++)
        *result = *result * x + va_arg(args, double);
    va_end(args);
    return OK;
}

static status_t parse_number(const char *text, const int base, ull *value)
{
    if (text == NULL || *text == '\0')
        return ERR_ARGUMENT;
    *value = 0;
    for (const char *p = text; *p != '\0'; p++) {
        int digit;

        if (*p >= '0' && *p <= '9')
            digit = *p - '0';
        else if (*p >= 'A' && *p <= 'Z')
            digit = *p - 'A' + 10;
        else if (*p >= 'a' && *p <= 'z')
            digit = *p - 'a' + 10;
        else
            return ERR_DIGIT;
        if (digit >= base)
            return ERR_DIGIT;
        if (*value > (~0ULL - digit) / base)
            return ERR_OVERFLOW;
        *value = *value * base + digit;
    }
    return OK;
}

static int is_kaprekar(const ull n, const int base)
{
    ull square = n * n;
    ull divisor = base;

    if (n == 0)
        return 0;
    for (;;) {
        ull right = square % divisor;
        ull left = square / divisor;

        if (right > 0 && left + right == n)
            return 1;
        if (divisor > square || divisor > ~0ULL / base)
            return 0;
        divisor *= base;
    }
}

static status_t find_kaprekar_numbers(const char **result, size_t *result_count, const int base, const int count, ...)
{
    va_list args;
    status_t status = OK;

    if (result == NULL || result_count == NULL || base < 2 || base > 36 || count < 0)
        return ERR_ARGUMENT;
    *result_count = 0;
    va_start(args, count);
    for (int i = 0; i < count && status == OK; i++) {
        const char *text = va_arg(args, const char *);
        ull value;

        status = parse_number(text, base, &value);
        if (status == OK && value > 0xFFFFFFFFULL)
            status = ERR_OVERFLOW;
        if (status == OK && is_kaprekar(value, base))
            result[(*result_count)++] = text;
    }
    va_end(args);
    return status;
}

static status_t geometric_mean(double *result, const int count, ...)
{
    va_list args;
    double log_sum = 0;
    int has_zero = 0;

    if (result == NULL || count <= 0)
        return ERR_ARGUMENT;
    va_start(args, count);
    for (int i = 0; i < count; i++) {
        double value = va_arg(args, double);

        if (value < 0) {
            va_end(args);
            return ERR_DOMAIN;
        }
        if (value == 0)
            has_zero = 1;
        else
            log_sum += log(value);
    }
    va_end(args);
    *result = has_zero ? 0 : exp(log_sum / count);
    return OK;
}

static double power_positive(const double base, const ull exponent)
{
    double half;

    if (exponent == 0)
        return 1;
    half = power_positive(base, exponent / 2);
    if (exponent % 2 == 0)
        return half * half;
    return half * half * base;
}

static status_t fast_power(double *result, const double base, const long long exponent)
{
    ull magnitude;

    if (result == NULL)
        return ERR_ARGUMENT;
    if (exponent >= 0) {
        *result = power_positive(base, (ull)exponent);
        return OK;
    }
    if (base == 0)
        return ERR_DOMAIN;
    magnitude = 0ULL - (ull)exponent;
    *result = 1 / power_positive(base, magnitude);
    return OK;
}

static status_t find_root(double *root, double left, double right, const double eps, const function_t f)
{
    double f_left;

    if (root == NULL || f == NULL || eps <= 0 || left >= right)
        return ERR_ARGUMENT;
    f_left = f(left);
    if (f_left * f(right) > 0)
        return ERR_INTERVAL;
    for (int i = 0; i < MAX_ROOT_ITERATIONS && right - left >= eps; i++) {
        double middle = (left + right) / 2;
        double f_middle = f(middle);

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

static double equation_square(const double x)
{
    return x * x - 2;
}

static double equation_cosine(const double x)
{
    return cos(x) - x;
}

static double equation_cubic(const double x)
{
    return x * x * x - x - 2;
}

static const char *status_message(const status_t status)
{
    switch (status) {
    case ERR_ARGUMENT: return "invalid argument";
    case ERR_MEMORY: return "memory allocation failed";
    case ERR_DIGIT: return "invalid digit for this base";
    case ERR_OVERFLOW: return "number is too large";
    case ERR_DOMAIN: return "argument outside of function domain";
    case ERR_INTERVAL: return "root is not bracketed by the interval";
    default: return "ok";
    }
}

static void demo_convex(void)
{
    int result;
    status_t status;

    status = is_convex_polygon(&result, 1e-9, 4, 0.0, 0.0, 4.0, 0.0, 4.0, 3.0, 0.0, 3.0);
    printf("rectangle: %s (%s)\n", result ? "convex" : "not convex", status_message(status));
    status = is_convex_polygon(&result, 1e-9, 5, 0.0, 0.0, 4.0, 0.0, 1.0, 1.0, 4.0, 4.0, 0.0, 4.0);
    printf("dent polygon: %s (%s)\n", result ? "convex" : "not convex", status_message(status));
    status = is_convex_polygon(&result, 1e-9, 5, 0.0, 3.0, 2.0, -3.0, -3.0, 1.0, 3.0, 1.0, -2.0, -3.0);
    printf("pentagram: %s (%s)\n", result ? "convex" : "not convex", status_message(status));
    status = is_convex_polygon(&result, 1e-9, 2, 0.0, 0.0, 1.0, 1.0);
    printf("two points: (%s)\n", status_message(status));
}

static void demo_polynomial(void)
{
    double value;

    evaluate_polynomial(&value, 2.0, 3, 1.0, -2.0, 0.0, 5.0);
    printf("x^3 - 2x^2 + 5 at 2 = %g\n", value);
    evaluate_polynomial(&value, -1.5, 0, 7.0);
    printf("constant 7 at -1.5 = %g\n", value);
    evaluate_polynomial(&value, 0.5, 2, 4.0, 0.0, -1.0);
    printf("4x^2 - 1 at 0.5 = %g\n", value);
}

static void demo_kaprekar(void)
{
    const char *found[8];
    size_t count;
    status_t status;

    status = find_kaprekar_numbers(found, &count, 10, 8, "1", "9", "10", "45", "55", "99", "100", "297");
    printf("base 10: %s\n", status_message(status));
    for (size_t i = 0; i < count; i++)
        printf("  %s\n", found[i]);
    status = find_kaprekar_numbers(found, &count, 16, 4, "1", "F", "2A", "FF");
    printf("base 16: %s\n", status_message(status));
    for (size_t i = 0; i < count; i++)
        printf("  %s\n", found[i]);
    status = find_kaprekar_numbers(found, &count, 2, 2, "101", "12");
    printf("base 2 with bad digit: %s\n", status_message(status));
}

static void demo_geometric_mean(void)
{
    double value;
    status_t status;

    status = geometric_mean(&value, 3, 1.0, 3.0, 9.0);
    printf("gm(1, 3, 9) = %g\n", value);
    status = geometric_mean(&value, 2, 4.0, 16.0);
    printf("gm(4, 16) = %g\n", value);
    status = geometric_mean(&value, 2, -1.0, 4.0);
    printf("gm(-1, 4): %s\n", status_message(status));
}

static void demo_power(void)
{
    double value;
    status_t status;

    fast_power(&value, 2.0, 10);
    printf("2^10 = %g\n", value);
    fast_power(&value, 2.0, -3);
    printf("2^-3 = %g\n", value);
    fast_power(&value, 1.5, 7);
    printf("1.5^7 = %g\n", value);
    fast_power(&value, -2.0, 5);
    printf("(-2)^5 = %g\n", value);
    status = fast_power(&value, 0.0, -1);
    printf("0^-1: %s\n", status_message(status));
}

static void demo_root(void)
{
    double root;
    status_t status;

    status = find_root(&root, 0.0, 2.0, 1e-3, equation_square);
    printf("x^2 - 2 on [0, 2], eps 1e-3: %.6f (%s)\n", root, status_message(status));
    status = find_root(&root, 0.0, 2.0, 1e-9, equation_square);
    printf("x^2 - 2 on [0, 2], eps 1e-9: %.9f (%s)\n", root, status_message(status));
    status = find_root(&root, 0.0, 1.0, 1e-6, equation_cosine);
    printf("cos x - x on [0, 1], eps 1e-6: %.6f (%s)\n", root, status_message(status));
    status = find_root(&root, 1.0, 2.0, 1e-6, equation_cubic);
    printf("x^3 - x - 2 on [1, 2], eps 1e-6: %.6f (%s)\n", root, status_message(status));
    status = find_root(&root, 2.0, 3.0, 1e-6, equation_square);
    printf("x^2 - 2 on [2, 3]: %s\n", status_message(status));
}

int main(void)
{
    printf("-- 1. convex polygon\n");
    demo_convex();
    printf("-- 2. polynomial\n");
    demo_polynomial();
    printf("-- 3. Kaprekar numbers\n");
    demo_kaprekar();
    printf("-- 4. geometric mean\n");
    demo_geometric_mean();
    printf("-- 5. fast power\n");
    demo_power();
    printf("-- 6. dichotomy\n");
    demo_root();
    return 0;
}
