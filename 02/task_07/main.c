#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    STATUS_OK,
    STATUS_NULL_ARGUMENT,
    STATUS_NO_MEMORY,
    STATUS_INVALID_ARGUMENT,
    STATUS_NOT_POSITIVE_DEFINITE
} status_t;

typedef struct {
    double *coords;
} vector_t;

typedef status_t (*norm_fn_t)(const double *coords, size_t dimension, const void *params, double *result);

typedef struct {
    double max_norm;
    vector_t *vectors;
    size_t count;
} norm_result_t;

typedef struct {
    double p;
    double eps;
} p_norm_params_t;

typedef struct {
    const double *matrix;
    double eps;
} matrix_norm_params_t;

status_t norm_inf(const double *coords, size_t dimension, const void *params, double *result)
{
    (void)params;
    if (coords == NULL || result == NULL) {
        return STATUS_NULL_ARGUMENT;
    }
    double max_value = 0.0;
    for (size_t i = 0; i < dimension; i++) {
        if (fabs(coords[i]) > max_value) {
            max_value = fabs(coords[i]);
        }
    }
    *result = max_value;
    return STATUS_OK;
}

status_t norm_p(const double *coords, size_t dimension, const void *params, double *result)
{
    if (coords == NULL || params == NULL || result == NULL) {
        return STATUS_NULL_ARGUMENT;
    }
    const p_norm_params_t *settings = params;
    if (settings->p < 1.0 - settings->eps) {
        return STATUS_INVALID_ARGUMENT;
    }
    double sum = 0.0;
    for (size_t i = 0; i < dimension; i++) {
        sum += pow(fabs(coords[i]), settings->p);
    }
    *result = pow(sum, 1.0 / settings->p);
    return STATUS_OK;
}

status_t norm_matrix(const double *coords, size_t dimension, const void *params, double *result)
{
    if (coords == NULL || params == NULL || result == NULL) {
        return STATUS_NULL_ARGUMENT;
    }
    const matrix_norm_params_t *settings = params;
    if (settings->matrix == NULL) {
        return STATUS_NULL_ARGUMENT;
    }
    double form = 0.0;
    for (size_t i = 0; i < dimension; i++) {
        double row_product = 0.0;
        for (size_t j = 0; j < dimension; j++) {
            row_product += settings->matrix[i * dimension + j] * coords[j];
        }
        form += row_product * coords[i];
    }
    if (form < -settings->eps) {
        return STATUS_NOT_POSITIVE_DEFINITE;
    }
    *result = form < 0.0 ? 0.0 : sqrt(form);
    return STATUS_OK;
}

void free_norm_results(norm_result_t *results, size_t count)
{
    if (results == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        free(results[i].vectors);
    }
    free(results);
}

static status_t compute_norm_result(const vector_t *vectors, size_t vector_count, size_t dimension,
                                    norm_fn_t function, const void *params, double eps,
                                    double *lengths, norm_result_t *result)
{
    double max_length = 0.0;
    for (size_t i = 0; i < vector_count; i++) {
        status_t status = function(vectors[i].coords, dimension, params, &lengths[i]);
        if (status != STATUS_OK) {
            return status;
        }
        if (i == 0 || lengths[i] - max_length > eps) {
            max_length = lengths[i];
        }
    }
    size_t count = 0;
    for (size_t i = 0; i < vector_count; i++) {
        if (fabs(lengths[i] - max_length) <= eps) {
            count++;
        }
    }
    result->vectors = malloc(count * sizeof(vector_t));
    if (result->vectors == NULL) {
        return STATUS_NO_MEMORY;
    }
    result->max_norm = max_length;
    result->count = 0;
    for (size_t i = 0; i < vector_count; i++) {
        if (fabs(lengths[i] - max_length) <= eps) {
            result->vectors[result->count++] = vectors[i];
        }
    }
    return STATUS_OK;
}

static status_t compute_all(const vector_t *vectors, size_t vector_count, size_t dimension,
                            const norm_fn_t *functions, const void *const *params, size_t norm_count,
                            double eps, norm_result_t *results)
{
    double *lengths = malloc(vector_count * sizeof(double));
    if (lengths == NULL) {
        return STATUS_NO_MEMORY;
    }
    status_t status = STATUS_OK;
    for (size_t i = 0; i < norm_count && status == STATUS_OK; i++) {
        status = compute_norm_result(vectors, vector_count, dimension, functions[i], params[i], eps,
                                     lengths, &results[i]);
    }
    free(lengths);
    return status;
}

status_t find_longest_vectors(size_t dimension, size_t vector_count, size_t norm_count, double eps,
                              norm_result_t **results, ...)
{
    if (results == NULL) {
        return STATUS_NULL_ARGUMENT;
    }
    if (dimension == 0 || vector_count == 0 || norm_count == 0 || eps < 0.0) {
        return STATUS_INVALID_ARGUMENT;
    }
    vector_t *vectors = malloc(vector_count * sizeof(vector_t));
    norm_fn_t *functions = malloc(norm_count * sizeof(norm_fn_t));
    const void **params = malloc(norm_count * sizeof(const void *));
    norm_result_t *output = calloc(norm_count, sizeof(norm_result_t));
    if (vectors == NULL || functions == NULL || params == NULL || output == NULL) {
        free(vectors);
        free(functions);
        free(params);
        free(output);
        return STATUS_NO_MEMORY;
    }
    va_list args;
    va_start(args, results);
    for (size_t i = 0; i < vector_count; i++) {
        vectors[i] = va_arg(args, vector_t);
    }
    for (size_t i = 0; i < norm_count; i++) {
        functions[i] = va_arg(args, norm_fn_t);
        params[i] = va_arg(args, const void *);
    }
    va_end(args);
    status_t status = STATUS_OK;
    for (size_t i = 0; i < vector_count && status == STATUS_OK; i++) {
        status = vectors[i].coords == NULL ? STATUS_NULL_ARGUMENT : STATUS_OK;
    }
    for (size_t i = 0; i < norm_count && status == STATUS_OK; i++) {
        status = functions[i] == NULL ? STATUS_NULL_ARGUMENT : STATUS_OK;
    }
    if (status == STATUS_OK) {
        status = compute_all(vectors, vector_count, dimension, functions, params, norm_count, eps, output);
    }
    free(vectors);
    free(functions);
    free(params);
    if (status != STATUS_OK) {
        free_norm_results(output, norm_count);
        return status;
    }
    *results = output;
    return STATUS_OK;
}

static const char *status_message(status_t status)
{
    switch (status) {
    case STATUS_NULL_ARGUMENT:
        return "null argument";
    case STATUS_NO_MEMORY:
        return "out of memory";
    case STATUS_INVALID_ARGUMENT:
        return "invalid argument";
    case STATUS_NOT_POSITIVE_DEFINITE:
        return "matrix is not positive definite";
    default:
        return "unknown error";
    }
}

static void print_results(const char *const *names, const norm_result_t *results, size_t norm_count,
                          size_t dimension)
{
    for (size_t i = 0; i < norm_count; i++) {
        printf("%s: max norm = %.4f, vectors: %zu\n", names[i], results[i].max_norm, results[i].count);
        for (size_t k = 0; k < results[i].count; k++) {
            printf("  (");
            for (size_t j = 0; j < dimension; j++) {
                printf(j == 0 ? "%g" : ", %g", results[i].vectors[k].coords[j]);
            }
            printf(")\n");
        }
    }
}

int main(void)
{
    double a1[] = {1, 2, 2};
    double a2[] = {3, 0, 0};
    double a3[] = {0, 3, 0};
    double a4[] = {-1, 1, 1};
    vector_t v1 = {a1};
    vector_t v2 = {a2};
    vector_t v3 = {a3};
    vector_t v4 = {a4};
    const double matrix[] = {2, 0, 0, 0, 1, 0, 0, 0, 1};
    p_norm_params_t p1 = {1.0, 1e-9};
    p_norm_params_t p2 = {2.0, 1e-9};
    matrix_norm_params_t a_params = {matrix, 1e-9};
    const char *names[] = {"inf-norm", "p=1", "p=2", "A-norm"};
    norm_result_t *results = NULL;

    status_t status = find_longest_vectors(3, 4, 4, 1e-9, &results, v1, v2, v3, v4,
                                           norm_inf, NULL, norm_p, &p1, norm_p, &p2, norm_matrix, &a_params);
    if (status != STATUS_OK) {
        fprintf(stderr, "Error: %s\n", status_message(status));
        return 1;
    }
    print_results(names, results, 4, 3);
    free_norm_results(results, 4);

    p_norm_params_t bad = {0.5, 1e-9};
    status = find_longest_vectors(3, 1, 1, 1e-9, &results, v1, norm_p, &bad);
    printf("p=0.5: %s\n", status_message(status));
    return 0;
}
