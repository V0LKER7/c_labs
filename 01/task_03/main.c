#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#define PERMUTATIONS_COUNT 6

typedef enum {
    OK = 0,
    ERR_ARGS_COUNT,
    ERR_FLAG,
    ERR_NUMBER,
    ERR_EPS,
    ERR_ZERO,
    ERR_SIDE
} status_t;

typedef enum {
    SOLUTION_NONE,
    SOLUTION_ONE,
    SOLUTION_TWO,
    SOLUTION_ANY
} solution_kind_t;

typedef struct {
    solution_kind_t kind;
    double roots[2];
} solution_t;

static status_t parse_flag(const char *arg, char *flag)
{
    if (arg == NULL || strlen(arg) != 2 || (arg[0] != '-' && arg[0] != '/'))
        return ERR_FLAG;
    *flag = arg[1];
    return OK;
}

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
    if (parse_double(text, eps) != OK || *eps <= 0)
        return ERR_EPS;
    return OK;
}

static status_t parse_integer(const char *text, long long *number)
{
    char *end;

    if (text == NULL || *text == '\0')
        return ERR_NUMBER;
    errno = 0;
    *number = strtoll(text, &end, 10);
    if (errno != 0 || *end != '\0')
        return ERR_NUMBER;
    return OK;
}

static solution_t solve_quadratic(const double a, const double b, const double c, const double eps)
{
    solution_t solution = {SOLUTION_NONE, {0, 0}};
    double discriminant;

    if (fabs(a) < eps) {
        if (fabs(b) < eps) {
            solution.kind = fabs(c) < eps ? SOLUTION_ANY : SOLUTION_NONE;
        } else {
            solution.kind = SOLUTION_ONE;
            solution.roots[0] = -c / b;
        }
        return solution;
    }
    discriminant = b * b - 4 * a * c;
    if (fabs(discriminant) < eps) {
        solution.kind = SOLUTION_ONE;
        solution.roots[0] = -b / (2 * a);
    } else if (discriminant > 0) {
        solution.kind = SOLUTION_TWO;
        solution.roots[0] = (-b - sqrt(discriminant)) / (2 * a);
        solution.roots[1] = (-b + sqrt(discriminant)) / (2 * a);
    }
    return solution;
}

static int same_triple(const double *first, const double *second, const double eps)
{
    for (int i = 0; i < 3; i++)
        if (fabs(first[i] - second[i]) >= eps)
            return 0;
    return 1;
}

static size_t unique_permutations(const double *coefficients, const double eps, double result[PERMUTATIONS_COUNT][3])
{
    static const int orders[PERMUTATIONS_COUNT][3] = {
        {0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}
    };
    size_t count = 0;

    for (int p = 0; p < PERMUTATIONS_COUNT; p++) {
        double candidate[3];
        int duplicate = 0;

        for (int i = 0; i < 3; i++)
            candidate[i] = coefficients[orders[p][i]];
        for (size_t j = 0; j < count; j++)
            if (same_triple(candidate, result[j], eps))
                duplicate = 1;
        if (!duplicate) {
            memcpy(result[count], candidate, sizeof(candidate));
            count++;
        }
    }
    return count;
}

static int is_divisible(const long long first, const long long second)
{
    if (second == -1)
        return 1;
    return first % second == 0;
}

static int is_right_triangle(const double *sides, const double eps)
{
    double largest = fmax(sides[0], fmax(sides[1], sides[2]));
    double sum_of_squares = sides[0] * sides[0] + sides[1] * sides[1] + sides[2] * sides[2];

    return fabs(2 * largest * largest - sum_of_squares) < eps;
}

static void print_solution(const solution_t *solution)
{
    switch (solution->kind) {
    case SOLUTION_NONE:
        printf("no real roots\n");
        break;
    case SOLUTION_ONE:
        printf("x = %g\n", solution->roots[0]);
        break;
    case SOLUTION_TWO:
        printf("x1 = %g, x2 = %g\n", solution->roots[0], solution->roots[1]);
        break;
    default:
        printf("any real number\n");
        break;
    }
}

static status_t run_quadratic(const int argc, char *argv[])
{
    double eps;
    double coefficients[3];
    double permutations[PERMUTATIONS_COUNT][3];
    size_t count;
    status_t status;

    if (argc != 6)
        return ERR_ARGS_COUNT;
    status = parse_eps(argv[2], &eps);
    for (int i = 0; status == OK && i < 3; i++)
        status = parse_double(argv[3 + i], &coefficients[i]);
    if (status != OK)
        return status;
    count = unique_permutations(coefficients, eps, permutations);
    for (size_t i = 0; i < count; i++) {
        solution_t solution = solve_quadratic(permutations[i][0], permutations[i][1], permutations[i][2], eps);

        printf("a=%g b=%g c=%g: ", permutations[i][0], permutations[i][1], permutations[i][2]);
        print_solution(&solution);
    }
    return OK;
}

static status_t run_multiple(const int argc, char *argv[])
{
    long long first;
    long long second;
    status_t status;

    if (argc != 4)
        return ERR_ARGS_COUNT;
    status = parse_integer(argv[2], &first);
    if (status == OK)
        status = parse_integer(argv[3], &second);
    if (status != OK)
        return status;
    if (first == 0 || second == 0)
        return ERR_ZERO;
    if (is_divisible(first, second))
        printf("%lld is divisible by %lld\n", first, second);
    else
        printf("%lld is not divisible by %lld\n", first, second);
    return OK;
}

static status_t run_triangle(const int argc, char *argv[])
{
    double eps;
    double sides[3];
    status_t status;

    if (argc != 6)
        return ERR_ARGS_COUNT;
    status = parse_eps(argv[2], &eps);
    for (int i = 0; status == OK && i < 3; i++)
        status = parse_double(argv[3 + i], &sides[i]);
    if (status != OK)
        return status;
    for (int i = 0; i < 3; i++)
        if (sides[i] <= eps)
            return ERR_SIDE;
    if (is_right_triangle(sides, eps))
        printf("these can be sides of a right triangle\n");
    else
        printf("these cannot be sides of a right triangle\n");
    return OK;
}

static const char *status_message(const status_t status)
{
    switch (status) {
    case ERR_ARGS_COUNT: return "Wrong number of arguments for this flag";
    case ERR_FLAG: return "Unknown flag";
    case ERR_NUMBER: return "Invalid number";
    case ERR_EPS: return "eps must be a positive number";
    case ERR_ZERO: return "Numbers must be non-zero";
    case ERR_SIDE: return "Sides must be positive";
    default: return "";
    }
}

int main(int argc, char *argv[])
{
    char flag;
    status_t status;

    if (argc < 2) {
        fprintf(stderr, "Usage: program <flag> <args>\n");
        return 1;
    }
    status = parse_flag(argv[1], &flag);
    if (status == OK) {
        switch (flag) {
        case 'q': status = run_quadratic(argc, argv); break;
        case 'm': status = run_multiple(argc, argv); break;
        case 't': status = run_triangle(argc, argv); break;
        default: status = ERR_FLAG; break;
        }
    }
    if (status != OK) {
        fprintf(stderr, "%s\n", status_message(status));
        return 1;
    }
    return 0;
}
