#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MIN_BASE 2
#define MAX_BASE 36
#define MAX_KAPREKAR_ARGUMENT 4294967295ULL

typedef enum {
    STATUS_OK,
    STATUS_INVALID_ARGUMENT,
    STATUS_MEMORY_ERROR,
    STATUS_INVALID_NUMBER,
    STATUS_OVERFLOW,
    STATUS_DOMAIN_ERROR,
    STATUS_INVALID_INTERVAL
} status_t;

typedef struct {
    double x;
    double y;
} point_t;

typedef double (*function_t)(double x);

static const char *status_message(status_t status)
{
    switch (status) {
    case STATUS_OK:
        return "ok";
    case STATUS_INVALID_ARGUMENT:
        return "invalid argument";
    case STATUS_MEMORY_ERROR:
        return "memory allocation failed";
    case STATUS_INVALID_NUMBER:
        return "invalid number representation";
    case STATUS_OVERFLOW:
        return "overflow";
    case STATUS_DOMAIN_ERROR:
        return "argument is outside of function domain";
    case STATUS_INVALID_INTERVAL:
        return "root is not bracketed by the interval";
    }
    return "unknown error";
}

static int polygon_is_convex(const point_t *vertices, int count, double eps)
{
    int has_positive = 0;
    int has_negative = 0;
    double turn_sum = 0.0;
    for (int index = 0; index < count; index++) {
        point_t first = vertices[index];
        point_t second = vertices[(index + 1) % count];
        point_t third = vertices[(index + 2) % count];
        double edge_x1 = second.x - first.x;
        double edge_y1 = second.y - first.y;
        double edge_x2 = third.x - second.x;
        double edge_y2 = third.y - second.y;
        double cross = edge_x1 * edge_y2 - edge_y1 * edge_x2;
        double dot = edge_x1 * edge_x2 + edge_y1 * edge_y2;
        if (cross > eps) {
            has_positive = 1;
        } else if (cross < -eps) {
            has_negative = 1;
        }
        turn_sum += atan2(cross, dot);
    }
    if (has_positive == has_negative) {
        return 0;
    }
    return fabs(fabs(turn_sum) - 2.0 * acos(-1.0)) < eps;
}

static status_t is_convex_polygon(int *result, double eps, int vertex_count, ...)
{
    if (result == NULL || eps <= 0.0 || vertex_count < 3) {
        return STATUS_INVALID_ARGUMENT;
    }
    point_t *vertices = malloc(sizeof(point_t) * (size_t)vertex_count);
    if (vertices == NULL) {
        return STATUS_MEMORY_ERROR;
    }
    va_list args;
    va_start(args, vertex_count);
    for (int index = 0; index < vertex_count; index++) {
        vertices[index].x = va_arg(args, double);
        vertices[index].y = va_arg(args, double);
    }
    va_end(args);
    *result = polygon_is_convex(vertices, vertex_count, eps);
    free(vertices);
    return STATUS_OK;
}

static status_t evaluate_polynomial(double *result, double x, int degree, ...)
{
    if (result == NULL || degree < 0 || !isfinite(x)) {
        return STATUS_INVALID_ARGUMENT;
    }
    va_list args;
    va_start(args, degree);
    double value = 0.0;
    for (int index = 0; index <= degree; index++) {
        value = value * x + va_arg(args, double);
    }
    va_end(args);
    if (!isfinite(value)) {
        return STATUS_OVERFLOW;
    }
    *result = value;
    return STATUS_OK;
}

static int digit_value(char symbol)
{
    if (symbol >= '0' && symbol <= '9') {
        return symbol - '0';
    }
    if (symbol >= 'A' && symbol <= 'Z') {
        return symbol - 'A' + 10;
    }
    if (symbol >= 'a' && symbol <= 'z') {
        return symbol - 'a' + 10;
    }
    return -1;
}

static status_t parse_in_base(const char *text, int base, uint64_t *value)
{
    if (text == NULL || value == NULL || *text == '\0' || base < MIN_BASE ||
        base > MAX_BASE) {
        return STATUS_INVALID_NUMBER;
    }
    uint64_t result = 0;
    for (const char *symbol = text; *symbol != '\0'; symbol++) {
        int digit = digit_value(*symbol);
        if (digit < 0 || digit >= base) {
            return STATUS_INVALID_NUMBER;
        }
        if (result > (UINT64_MAX - (uint64_t)digit) / (uint64_t)base) {
            return STATUS_OVERFLOW;
        }
        result = result * (uint64_t)base + (uint64_t)digit;
    }
    *value = result;
    return STATUS_OK;
}

static int number_is_kaprekar(uint64_t number, int base)
{
    if (number == 1) {
        return 1;
    }
    uint64_t square = number * number;
    uint64_t power = (uint64_t)base;
    while (power <= square) {
        uint64_t right = square % power;
        uint64_t left = square / power;
        if (right > 0 && left + right == number) {
            return 1;
        }
        if (power > square / (uint64_t)base) {
            break;
        }
        power *= (uint64_t)base;
    }
    return 0;
}

static status_t find_kaprekar_numbers(const char **found, int *found_count,
                                      int base, int count, ...)
{
    if (found == NULL || found_count == NULL || count < 0 || base < MIN_BASE ||
        base > MAX_BASE) {
        return STATUS_INVALID_ARGUMENT;
    }
    va_list args;
    va_start(args, count);
    status_t status = STATUS_OK;
    int total = 0;
    for (int index = 0; index < count && status == STATUS_OK; index++) {
        const char *text = va_arg(args, const char *);
        uint64_t number = 0;
        status = parse_in_base(text, base, &number);
        if (status == STATUS_OK && number > MAX_KAPREKAR_ARGUMENT) {
            status = STATUS_OVERFLOW;
        }
        if (status == STATUS_OK && number > 0 && number_is_kaprekar(number, base)) {
            found[total++] = text;
        }
    }
    va_end(args);
    if (status == STATUS_OK) {
        *found_count = total;
    }
    return status;
}

static status_t geometric_mean(double *result, int count, ...)
{
    if (result == NULL || count <= 0) {
        return STATUS_INVALID_ARGUMENT;
    }
    va_list args;
    va_start(args, count);
    double log_sum = 0.0;
    int has_zero = 0;
    status_t status = STATUS_OK;
    for (int index = 0; index < count; index++) {
        double value = va_arg(args, double);
        if (!isfinite(value) || value < 0.0) {
            status = STATUS_DOMAIN_ERROR;
        } else if (value == 0.0) {
            has_zero = 1;
        } else {
            log_sum += log(value);
        }
    }
    va_end(args);
    if (status != STATUS_OK) {
        return status;
    }
    *result = has_zero ? 0.0 : exp(log_sum / (double)count);
    return STATUS_OK;
}

static status_t power_recursive(double base, long long exponent, double *result)
{
    if (result == NULL || !isfinite(base)) {
        return STATUS_INVALID_ARGUMENT;
    }
    if (exponent < 0) {
        if (base == 0.0) {
            return STATUS_DOMAIN_ERROR;
        }
        double positive = 0.0;
        status_t status = power_recursive(base, -exponent, &positive);
        if (status != STATUS_OK) {
            return status;
        }
        *result = 1.0 / positive;
        return STATUS_OK;
    }
    if (exponent == 0) {
        *result = 1.0;
        return STATUS_OK;
    }
    double half = 0.0;
    status_t status = power_recursive(base, exponent / 2, &half);
    if (status != STATUS_OK) {
        return status;
    }
    double value = half * half;
    if (exponent % 2 != 0) {
        value *= base;
    }
    if (!isfinite(value)) {
        return STATUS_OVERFLOW;
    }
    *result = value;
    return STATUS_OK;
}

static status_t find_root_bisection(double left, double right, double eps,
                                    function_t function, double *root)
{
    if (function == NULL || root == NULL || eps <= 0.0 || left >= right) {
        return STATUS_INVALID_ARGUMENT;
    }
    double value_left = function(left);
    double value_right = function(right);
    int left_positive = value_left > 0.0;
    if (value_left != 0.0 && value_right != 0.0 &&
        left_positive == (value_right > 0.0)) {
        return STATUS_INVALID_INTERVAL;
    }
    while (right - left > eps) {
        double middle = left + (right - left) / 2.0;
        if (middle <= left || middle >= right) {
            break;
        }
        double value_middle = function(middle);
        if (value_middle == 0.0) {
            *root = middle;
            return STATUS_OK;
        }
        if ((value_middle > 0.0) == left_positive) {
            left = middle;
        } else {
            right = middle;
        }
    }
    *root = left + (right - left) / 2.0;
    return STATUS_OK;
}

static double equation_square(double x)
{
    return x * x - 2.0;
}

static double equation_cosine(double x)
{
    return cos(x) - x;
}

static double equation_cubic(double x)
{
    return x * x * x - x - 2.0;
}

static void print_convex_result(const char *name, status_t status, int result)
{
    if (status == STATUS_OK) {
        printf("%-10s %s\n", name, result ? "convex" : "not convex");
    } else {
        printf("%-10s error: %s\n", name, status_message(status));
    }
}

static void demo_convex(void)
{
    int result = 0;
    status_t status = is_convex_polygon(&result, 1e-9, 4, 0.0, 0.0, 4.0, 0.0, 4.0,
                                        4.0, 0.0, 4.0);
    print_convex_result("square", status, result);
    status = is_convex_polygon(&result, 1e-9, 4, 0.0, 0.0, 4.0, 0.0, 1.0, 1.0, 0.0,
                               4.0);
    print_convex_result("arrow", status, result);
    status = is_convex_polygon(&result, 1e-9, 5, 0.0, 3.0, 2.0, -2.0, -2.0, 1.0,
                               2.0, 1.0, -2.0, -2.0);
    print_convex_result("pentagram", status, result);
    status = is_convex_polygon(&result, 1e-9, 2, 0.0, 0.0, 1.0, 1.0);
    print_convex_result("two points", status, result);
}

static void demo_polynomial(void)
{
    double value = 0.0;
    status_t status = evaluate_polynomial(&value, 2.0, 3, 1.0, -2.0, 0.0, 5.0);
    if (status == STATUS_OK) {
        printf("x^3 - 2x^2 + 5 at x=2: %g\n", value);
    }
    status = evaluate_polynomial(&value, 0.5, 0, 7.0);
    if (status == STATUS_OK) {
        printf("constant 7 at x=0.5: %g\n", value);
    }
    status = evaluate_polynomial(&value, 1.0, -1);
    printf("negative degree: %s\n", status_message(status));
}

static void demo_kaprekar(void)
{
    const char *found[8];
    int found_count = 0;
    status_t status = find_kaprekar_numbers(found, &found_count, 10, 7, "1", "9",
                                            "10", "45", "55", "297", "100");
    if (status == STATUS_OK) {
        printf("base 10 kaprekar:");
        for (int index = 0; index < found_count; index++) {
            printf(" %s", found[index]);
        }
        printf("\n");
    }
    status = find_kaprekar_numbers(found, &found_count, 16, 3, "FF", "10", "1");
    if (status == STATUS_OK) {
        printf("base 16 kaprekar:");
        for (int index = 0; index < found_count; index++) {
            printf(" %s", found[index]);
        }
        printf("\n");
    }
    status = find_kaprekar_numbers(found, &found_count, 10, 2, "45", "4x");
    printf("invalid digit: %s\n", status_message(status));
}

static void demo_geometric_mean(void)
{
    double value = 0.0;
    status_t status = geometric_mean(&value, 3, 1.0, 3.0, 9.0);
    if (status == STATUS_OK) {
        printf("geometric mean of 1 3 9: %g\n", value);
    }
    status = geometric_mean(&value, 2, 4.0, 9.0);
    if (status == STATUS_OK) {
        printf("geometric mean of 4 9: %g\n", value);
    }
    status = geometric_mean(&value, 2, -4.0, 9.0);
    printf("negative number: %s\n", status_message(status));
}

static void demo_power(void)
{
    double value = 0.0;
    status_t status = power_recursive(2.0, 10, &value);
    if (status == STATUS_OK) {
        printf("2^10 = %g\n", value);
    }
    status = power_recursive(2.0, -3, &value);
    if (status == STATUS_OK) {
        printf("2^-3 = %g\n", value);
    }
    status = power_recursive(1.5, 7, &value);
    if (status == STATUS_OK) {
        printf("1.5^7 = %g\n", value);
    }
    status = power_recursive(0.0, -1, &value);
    printf("0^-1: %s\n", status_message(status));
    status = power_recursive(10.0, 1000, &value);
    printf("10^1000: %s\n", status_message(status));
}

static void demo_bisection(void)
{
    const double eps_values[] = {1e-2, 1e-9};
    for (size_t index = 0; index < 2; index++) {
        double root = 0.0;
        double eps = eps_values[index];
        status_t status = find_root_bisection(0.0, 2.0, eps, equation_square, &root);
        if (status == STATUS_OK) {
            printf("x^2-2=0 on [0,2], eps=%g: %.10f\n", eps, root);
        }
        status = find_root_bisection(0.0, 1.0, eps, equation_cosine, &root);
        if (status == STATUS_OK) {
            printf("cos(x)-x=0 on [0,1], eps=%g: %.10f\n", eps, root);
        }
        status = find_root_bisection(1.0, 2.0, eps, equation_cubic, &root);
        if (status == STATUS_OK) {
            printf("x^3-x-2=0 on [1,2], eps=%g: %.10f\n", eps, root);
        }
    }
    double root = 0.0;
    status_t status = find_root_bisection(2.0, 3.0, 1e-6, equation_square, &root);
    printf("x^2-2=0 on [2,3]: %s\n", status_message(status));
}

int main(void)
{
    printf("-- 6.1 convex polygon\n");
    demo_convex();
    printf("-- 6.2 polynomial\n");
    demo_polynomial();
    printf("-- 6.3 kaprekar numbers\n");
    demo_kaprekar();
    printf("-- 6.4 geometric mean\n");
    demo_geometric_mean();
    printf("-- 6.5 fast power\n");
    demo_power();
    printf("-- 6.6 bisection\n");
    demo_bisection();
    return 0;
}
