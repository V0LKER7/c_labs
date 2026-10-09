#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    STATUS_OK,
    STATUS_NULL_POINTER,
    STATUS_NO_MEMORY,
    STATUS_INVALID_ARGUMENT,
    STATUS_NOT_POSITIVE_DEFINITE
} Status;

typedef struct {
    const double *coords;
} Vector;

typedef Status (*Norm_function)(const double *coords, size_t n, const void *params, double *result);

typedef struct {
    double norm;
    size_t *indices;
    size_t count;
} Longest_result;

Status norm_infinity(const double *coords, size_t n, const void *params, double *result)
{
    (void)params;
    if (coords == NULL || result == NULL) {
        return STATUS_NULL_POINTER;
    }
    double maximum = 0.0;
    for (size_t i = 0; i < n; i++) {
        if (fabs(coords[i]) > maximum) {
            maximum = fabs(coords[i]);
        }
    }
    *result = maximum;
    return STATUS_OK;
}

Status norm_p(const double *coords, size_t n, const void *params, double *result)
{
    if (coords == NULL || params == NULL || result == NULL) {
        return STATUS_NULL_POINTER;
    }
    double p = *(const double *)params;
    if (p < 1.0) {
        return STATUS_INVALID_ARGUMENT;
    }
    double sum = 0.0;
    for (size_t i = 0; i < n; i++) {
        sum += pow(fabs(coords[i]), p);
    }
    *result = pow(sum, 1.0 / p);
    return STATUS_OK;
}

Status norm_matrix(const double *coords, size_t n, const void *params, double *result)
{
    if (coords == NULL || params == NULL || result == NULL) {
        return STATUS_NULL_POINTER;
    }
    const double *matrix = params;
    double sum = 0.0;
    for (size_t i = 0; i < n; i++) {
        double row = 0.0;
        for (size_t j = 0; j < n; j++) {
            row += matrix[i * n + j] * coords[j];
        }
        sum += row * coords[i];
    }
    if (sum < 0.0) {
        return STATUS_NOT_POSITIVE_DEFINITE;
    }
    *result = sqrt(sum);
    return STATUS_OK;
}

Status check_positive_definite(const double *matrix, size_t n, double epsilon)
{
    if (matrix == NULL) {
        return STATUS_NULL_POINTER;
    }
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < i; j++) {
            if (fabs(matrix[i * n + j] - matrix[j * n + i]) > epsilon) {
                return STATUS_NOT_POSITIVE_DEFINITE;
            }
        }
    }
    double *lower = calloc(n * n, sizeof *lower);
    if (lower == NULL) {
        return STATUS_NO_MEMORY;
    }
    Status status = STATUS_OK;
    for (size_t i = 0; i < n && status == STATUS_OK; i++) {
        for (size_t j = 0; j <= i && status == STATUS_OK; j++) {
            double sum = matrix[i * n + j];
            for (size_t k = 0; k < j; k++) {
                sum -= lower[i * n + k] * lower[j * n + k];
            }
            if (i == j) {
                if (sum <= epsilon) {
                    status = STATUS_NOT_POSITIVE_DEFINITE;
                } else {
                    lower[i * n + i] = sqrt(sum);
                }
            } else {
                lower[i * n + j] = sum / lower[j * n + j];
            }
        }
    }
    free(lower);
    return status;
}

void free_longest_results(Longest_result *results, size_t count)
{
    if (results == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        free(results[i].indices);
    }
    free(results);
}

static Status compute_longest(const Vector *vectors, size_t vector_count, size_t n,
                              Norm_function function, const void *params, double epsilon,
                              double *norms, Longest_result *output)
{
    if (function == NULL) {
        return STATUS_NULL_POINTER;
    }
    double maximum = 0.0;
    for (size_t i = 0; i < vector_count; i++) {
        Status status = function(vectors[i].coords, n, params, &norms[i]);
        if (status != STATUS_OK) {
            return status;
        }
        if (i == 0 || norms[i] > maximum) {
            maximum = norms[i];
        }
    }
    size_t count = 0;
    for (size_t i = 0; i < vector_count; i++) {
        if (maximum - norms[i] <= epsilon) {
            count++;
        }
    }
    size_t *indices = malloc(count * sizeof *indices);
    if (indices == NULL) {
        return STATUS_NO_MEMORY;
    }
    size_t position = 0;
    for (size_t i = 0; i < vector_count; i++) {
        if (maximum - norms[i] <= epsilon) {
            indices[position++] = i;
        }
    }
    output->norm = maximum;
    output->indices = indices;
    output->count = count;
    return STATUS_OK;
}

Status find_longest_vectors(double epsilon, size_t n, size_t vector_count, size_t norm_count,
                            Longest_result **results, ...)
{
    if (results == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (epsilon < 0.0 || n == 0 || vector_count == 0 || norm_count == 0) {
        return STATUS_INVALID_ARGUMENT;
    }
    Vector *vectors = malloc(vector_count * sizeof *vectors);
    double *norms = malloc(vector_count * sizeof *norms);
    Longest_result *found = calloc(norm_count, sizeof *found);
    if (vectors == NULL || norms == NULL || found == NULL) {
        free(vectors);
        free(norms);
        free(found);
        return STATUS_NO_MEMORY;
    }
    va_list args;
    va_start(args, results);
    for (size_t i = 0; i < vector_count; i++) {
        vectors[i] = va_arg(args, Vector);
    }
    Status status = STATUS_OK;
    for (size_t k = 0; k < norm_count && status == STATUS_OK; k++) {
        Norm_function function = va_arg(args, Norm_function);
        const void *params = va_arg(args, const void *);
        status = compute_longest(vectors, vector_count, n, function, params, epsilon, norms, &found[k]);
    }
    va_end(args);
    free(vectors);
    free(norms);
    if (status != STATUS_OK) {
        free_longest_results(found, norm_count);
        return status;
    }
    *results = found;
    return STATUS_OK;
}

static void print_results(const char *const *names, const Longest_result *results, size_t count)
{
    for (size_t k = 0; k < count; k++) {
        printf("%s: norm=%.4f, vectors:", names[k], results[k].norm);
        for (size_t i = 0; i < results[k].count; i++) {
            printf(" v%zu", results[k].indices[i] + 1);
        }
        printf("\n");
    }
}

int main(void)
{
    const double c1[] = {1, 2, 3};
    const double c2[] = {-4, 0, 1};
    const double c3[] = {2, -4, 0};
    const double p1 = 1.0;
    const double p2 = 2.0;
    const double matrix[] = {1, 0, 0, 0, 2, 0, 0, 0, 3};
    const double bad_matrix[] = {1, 2, 0, 2, 1, 0, 0, 0, 1};
    Vector v1 = {c1};
    Vector v2 = {c2};
    Vector v3 = {c3};

    printf("A positive definite: %s\n", check_positive_definite(matrix, 3, 1e-12) == STATUS_OK ? "yes" : "no");
    printf("Bad A positive definite: %s\n", check_positive_definite(bad_matrix, 3, 1e-12) == STATUS_OK ? "yes" : "no");

    const char *names[] = {"infinity", "p=1", "p=2", "A=diag(1,2,3)"};
    Longest_result *results = NULL;
    Status status = find_longest_vectors(1e-9, 3, 3, 4, &results, v1, v2, v3,
                                         norm_infinity, (const void *)NULL,
                                         norm_p, (const void *)&p1,
                                         norm_p, (const void *)&p2,
                                         norm_matrix, (const void *)matrix);
    if (status != STATUS_OK) {
        printf("Error: status %d\n", status);
        return 1;
    }
    print_results(names, results, 4);
    free_longest_results(results, 4);

    const double bad_p = 0.5;
    status = find_longest_vectors(1e-9, 3, 3, 1, &results, v1, v2, v3, norm_p, (const void *)&bad_p);
    printf("p=0.5 status: %d\n", status);
    return 0;
}
