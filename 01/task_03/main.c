#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define COEFFICIENTS_COUNT 3
#define PERMUTATIONS_COUNT 6

typedef enum {
    STATUS_OK,
    STATUS_INVALID_ARGS_COUNT,
    STATUS_INVALID_FLAG,
    STATUS_INVALID_NUMBER,
    STATUS_INVALID_EPS,
    STATUS_INVALID_VALUE,
    STATUS_OVERFLOW
} status_t;

typedef enum {
    SOLUTION_NONE,
    SOLUTION_ANY,
    SOLUTION_ONE,
    SOLUTION_TWO_REAL,
    SOLUTION_COMPLEX_PAIR
} solution_kind_t;

typedef struct {
    solution_kind_t kind;
    double first;
    double second;
} solution_t;

static const char *status_message(status_t status)
{
    switch (status) {
    case STATUS_OK:
        return "ok";
    case STATUS_INVALID_ARGS_COUNT:
        return "wrong number of arguments for this flag";
    case STATUS_INVALID_FLAG:
        return "unknown flag";
    case STATUS_INVALID_NUMBER:
        return "argument is not a valid number";
    case STATUS_INVALID_EPS:
        return "epsilon must be a positive number";
    case STATUS_INVALID_VALUE:
        return "value is not allowed for this flag";
    case STATUS_OVERFLOW:
        return "numeric overflow";
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

static status_t parse_eps(const char *text, double *eps)
{
    double value = 0.0;
    status_t status = parse_double(text, &value);
    if (status != STATUS_OK) {
        return status;
    }
    if (value <= 0.0) {
        return STATUS_INVALID_EPS;
    }
    *eps = value;
    return STATUS_OK;
}

static status_t parse_int64(const char *text, int64_t *value)
{
    if (text == NULL || value == NULL || *text == '\0') {
        return STATUS_INVALID_NUMBER;
    }
    char *end = NULL;
    errno = 0;
    long long result = strtoll(text, &end, 10);
    if (errno == ERANGE || *end != '\0') {
        return STATUS_INVALID_NUMBER;
    }
    *value = (int64_t)result;
    return STATUS_OK;
}

static status_t parse_flag(const char *argument, char *flag)
{
    if (argument == NULL || flag == NULL) {
        return STATUS_INVALID_FLAG;
    }
    if ((argument[0] != '-' && argument[0] != '/') || argument[1] == '\0' ||
        argument[2] != '\0') {
        return STATUS_INVALID_FLAG;
    }
    *flag = argument[1];
    return STATUS_OK;
}

static int is_zero(double value, double eps)
{
    return fabs(value) < eps;
}

static status_t solve_quadratic(double a, double b, double c, double eps,
                                solution_t *solution)
{
    if (solution == NULL || eps <= 0.0) {
        return STATUS_INVALID_EPS;
    }
    if (is_zero(a, eps)) {
        if (is_zero(b, eps)) {
            solution->kind = is_zero(c, eps) ? SOLUTION_ANY : SOLUTION_NONE;
        } else {
            solution->kind = SOLUTION_ONE;
            solution->first = -c / b + 0.0;
        }
        return STATUS_OK;
    }
    double discriminant = b * b - 4.0 * a * c;
    if (!isfinite(discriminant)) {
        return STATUS_OVERFLOW;
    }
    if (is_zero(discriminant, eps)) {
        solution->kind = SOLUTION_ONE;
        solution->first = -b / (2.0 * a) + 0.0;
    } else if (discriminant > 0.0) {
        double root = sqrt(discriminant);
        solution->kind = SOLUTION_TWO_REAL;
        solution->first = (-b - root) / (2.0 * a);
        solution->second = (-b + root) / (2.0 * a);
    } else {
        solution->kind = SOLUTION_COMPLEX_PAIR;
        solution->first = -b / (2.0 * a) + 0.0;
        solution->second = sqrt(-discriminant) / (2.0 * fabs(a));
    }
    return STATUS_OK;
}

static int same_triple(const double *left, const double *right, double eps)
{
    for (int index = 0; index < COEFFICIENTS_COUNT; index++) {
        if (fabs(left[index] - right[index]) >= eps) {
            return 0;
        }
    }
    return 1;
}

static status_t unique_permutations(const double *coefficients, double eps,
                                    double result[PERMUTATIONS_COUNT][COEFFICIENTS_COUNT],
                                    size_t *count)
{
    if (coefficients == NULL || result == NULL || count == NULL || eps <= 0.0) {
        return STATUS_INVALID_VALUE;
    }
    const int orders[PERMUTATIONS_COUNT][COEFFICIENTS_COUNT] = {
        {0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    *count = 0;
    for (int order = 0; order < PERMUTATIONS_COUNT; order++) {
        double candidate[COEFFICIENTS_COUNT];
        for (int index = 0; index < COEFFICIENTS_COUNT; index++) {
            candidate[index] = coefficients[orders[order][index]];
        }
        int duplicate = 0;
        for (size_t known = 0; known < *count; known++) {
            if (same_triple(candidate, result[known], eps)) {
                duplicate = 1;
                break;
            }
        }
        if (!duplicate) {
            for (int index = 0; index < COEFFICIENTS_COUNT; index++) {
                result[*count][index] = candidate[index];
            }
            (*count)++;
        }
    }
    return STATUS_OK;
}

static status_t is_multiple(int64_t first, int64_t second, int *result)
{
    if (result == NULL) {
        return STATUS_INVALID_VALUE;
    }
    if (first == 0 || second == 0) {
        return STATUS_INVALID_VALUE;
    }
    if (second == -1) {
        *result = 1;
        return STATUS_OK;
    }
    *result = (first % second == 0);
    return STATUS_OK;
}

static status_t is_right_triangle(double a, double b, double c, double eps,
                                  int *result)
{
    if (result == NULL || eps <= 0.0) {
        return STATUS_INVALID_EPS;
    }
    if (a <= 0.0 || b <= 0.0 || c <= 0.0) {
        return STATUS_INVALID_VALUE;
    }
    double aa = a * a;
    double bb = b * b;
    double cc = c * c;
    if (!isfinite(aa) || !isfinite(bb) || !isfinite(cc)) {
        return STATUS_OVERFLOW;
    }
    *result = is_zero(aa + bb - cc, eps) || is_zero(aa + cc - bb, eps) ||
              is_zero(bb + cc - aa, eps);
    return STATUS_OK;
}

static void print_solution(const solution_t *solution)
{
    switch (solution->kind) {
    case SOLUTION_NONE:
        printf("no solutions\n");
        break;
    case SOLUTION_ANY:
        printf("any real x is a solution\n");
        break;
    case SOLUTION_ONE:
        printf("x = %g\n", solution->first);
        break;
    case SOLUTION_TWO_REAL:
        printf("x1 = %g, x2 = %g\n", solution->first, solution->second);
        break;
    case SOLUTION_COMPLEX_PAIR:
        printf("x1 = %g + %gi, x2 = %g - %gi\n", solution->first,
               solution->second, solution->first, solution->second);
        break;
    }
}

static status_t run_quadratic(char *const *values)
{
    double eps = 0.0;
    double coefficients[COEFFICIENTS_COUNT];
    status_t status = parse_eps(values[0], &eps);
    for (int index = 0; status == STATUS_OK && index < COEFFICIENTS_COUNT;
         index++) {
        status = parse_double(values[index + 1], &coefficients[index]);
    }
    if (status != STATUS_OK) {
        return status;
    }
    double permutations[PERMUTATIONS_COUNT][COEFFICIENTS_COUNT];
    size_t count = 0;
    status = unique_permutations(coefficients, eps, permutations, &count);
    if (status != STATUS_OK) {
        return status;
    }
    for (size_t index = 0; index < count; index++) {
        solution_t solution = {SOLUTION_NONE, 0.0, 0.0};
        status = solve_quadratic(permutations[index][0], permutations[index][1],
                                 permutations[index][2], eps, &solution);
        if (status != STATUS_OK) {
            return status;
        }
        printf("a=%g b=%g c=%g: ", permutations[index][0],
               permutations[index][1], permutations[index][2]);
        print_solution(&solution);
    }
    return STATUS_OK;
}

static status_t run_multiple(char *const *values)
{
    int64_t first = 0;
    int64_t second = 0;
    status_t status = parse_int64(values[0], &first);
    if (status == STATUS_OK) {
        status = parse_int64(values[1], &second);
    }
    int result = 0;
    if (status == STATUS_OK) {
        status = is_multiple(first, second, &result);
    }
    if (status != STATUS_OK) {
        return status;
    }
    printf("%lld %s a multiple of %lld\n", (long long)first,
           result ? "is" : "is not", (long long)second);
    return STATUS_OK;
}

static status_t run_triangle(char *const *values)
{
    double eps = 0.0;
    double sides[COEFFICIENTS_COUNT];
    status_t status = parse_eps(values[0], &eps);
    for (int index = 0; status == STATUS_OK && index < COEFFICIENTS_COUNT;
         index++) {
        status = parse_double(values[index + 1], &sides[index]);
    }
    int result = 0;
    if (status == STATUS_OK) {
        status = is_right_triangle(sides[0], sides[1], sides[2], eps, &result);
    }
    if (status != STATUS_OK) {
        return status;
    }
    printf("sides %g %g %g %s form a right triangle\n", sides[0], sides[1],
           sides[2], result ? "can" : "cannot");
    return STATUS_OK;
}

static status_t run_command(char flag, int argc, char *const *values)
{
    switch (flag) {
    case 'q':
        return argc == 6 ? run_quadratic(values) : STATUS_INVALID_ARGS_COUNT;
    case 'm':
        return argc == 4 ? run_multiple(values) : STATUS_INVALID_ARGS_COUNT;
    case 't':
        return argc == 6 ? run_triangle(values) : STATUS_INVALID_ARGS_COUNT;
    default:
        return STATUS_INVALID_FLAG;
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr,
                "Usage: %s -q eps a b c | -m n1 n2 | -t eps s1 s2 s3\n",
                argv[0]);
        return 1;
    }
    char flag = '\0';
    status_t status = parse_flag(argv[1], &flag);
    if (status == STATUS_OK) {
        status = run_command(flag, argc, argv + 2);
    }
    if (status != STATUS_OK) {
        fprintf(stderr, "Error: %s\n", status_message(status));
        return 1;
    }
    return 0;
}
