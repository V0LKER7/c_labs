#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define FIXED_ARRAY_SIZE 20
#define MIN_DYNAMIC_SIZE 10
#define MAX_DYNAMIC_SIZE 10000
#define MIN_ELEMENT -1000
#define MAX_ELEMENT 1000
#define MAX_ABS_BOUND 1000000000LL
#define PRINT_LIMIT 10

typedef enum {
    STATUS_OK,
    STATUS_INVALID_ARGS_COUNT,
    STATUS_INVALID_NUMBER,
    STATUS_INVALID_RANGE,
    STATUS_INVALID_ARGUMENT,
    STATUS_MEMORY_ERROR
} status_t;

static const char *status_message(status_t status)
{
    switch (status) {
    case STATUS_OK:
        return "ok";
    case STATUS_INVALID_ARGS_COUNT:
        return "wrong number of arguments";
    case STATUS_INVALID_NUMBER:
        return "bounds must be integers";
    case STATUS_INVALID_RANGE:
        return "bounds must satisfy a <= b and |a|, |b| <= 1000000000";
    case STATUS_INVALID_ARGUMENT:
        return "invalid argument";
    case STATUS_MEMORY_ERROR:
        return "memory allocation failed";
    }
    return "unknown error";
}

static status_t parse_bound(const char *text, long long *value)
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
    *value = result;
    return STATUS_OK;
}

static int random_in_range(int low, int high)
{
    double fraction = (double)rand() / ((double)RAND_MAX + 1.0);
    return low + (int)(fraction * ((double)high - (double)low + 1.0));
}

static status_t fill_random(int *array, size_t size, int low, int high)
{
    if (array == NULL || low > high) {
        return STATUS_INVALID_ARGUMENT;
    }
    for (size_t index = 0; index < size; index++) {
        array[index] = random_in_range(low, high);
    }
    return STATUS_OK;
}

static status_t swap_min_max(int *array, size_t size)
{
    if (array == NULL || size == 0) {
        return STATUS_INVALID_ARGUMENT;
    }
    size_t min_index = 0;
    size_t max_index = 0;
    for (size_t index = 1; index < size; index++) {
        if (array[index] < array[min_index]) {
            min_index = index;
        }
        if (array[index] > array[max_index]) {
            max_index = index;
        }
    }
    int buffer = array[min_index];
    array[min_index] = array[max_index];
    array[max_index] = buffer;
    return STATUS_OK;
}

static int absolute_difference(int first, int second)
{
    return first > second ? first - second : second - first;
}

static status_t build_sum_with_nearest(const int *first, size_t first_size,
                                       const int *second, size_t second_size,
                                       int **result)
{
    if (first == NULL || second == NULL || result == NULL || first_size == 0 ||
        second_size == 0) {
        return STATUS_INVALID_ARGUMENT;
    }
    int *sums = malloc(first_size * sizeof(int));
    if (sums == NULL) {
        return STATUS_MEMORY_ERROR;
    }
    for (size_t index = 0; index < first_size; index++) {
        int nearest = second[0];
        for (size_t other = 1; other < second_size; other++) {
            if (absolute_difference(second[other], first[index]) <
                absolute_difference(nearest, first[index])) {
                nearest = second[other];
            }
        }
        sums[index] = first[index] + nearest;
    }
    *result = sums;
    return STATUS_OK;
}

static void print_array(const char *name, const int *array, size_t size,
                        size_t limit)
{
    printf("%s (size %zu):", name, size);
    for (size_t index = 0; index < size && index < limit; index++) {
        printf(" %d", array[index]);
    }
    printf("%s\n", size > limit ? " ..." : "");
}

static status_t run_fixed_array(int low, int high)
{
    int array[FIXED_ARRAY_SIZE];
    status_t status = fill_random(array, FIXED_ARRAY_SIZE, low, high);
    if (status != STATUS_OK) {
        return status;
    }
    print_array("before", array, FIXED_ARRAY_SIZE, FIXED_ARRAY_SIZE);
    status = swap_min_max(array, FIXED_ARRAY_SIZE);
    if (status != STATUS_OK) {
        return status;
    }
    print_array("after ", array, FIXED_ARRAY_SIZE, FIXED_ARRAY_SIZE);
    return STATUS_OK;
}

static status_t run_dynamic_arrays(void)
{
    size_t first_size = (size_t)random_in_range(MIN_DYNAMIC_SIZE, MAX_DYNAMIC_SIZE);
    size_t second_size = (size_t)random_in_range(MIN_DYNAMIC_SIZE, MAX_DYNAMIC_SIZE);
    int *first = malloc(first_size * sizeof(int));
    int *second = malloc(second_size * sizeof(int));
    int *sums = NULL;
    status_t status = (first == NULL || second == NULL) ? STATUS_MEMORY_ERROR
                                                       : STATUS_OK;
    if (status == STATUS_OK) {
        status = fill_random(first, first_size, MIN_ELEMENT, MAX_ELEMENT);
    }
    if (status == STATUS_OK) {
        status = fill_random(second, second_size, MIN_ELEMENT, MAX_ELEMENT);
    }
    if (status == STATUS_OK) {
        status = build_sum_with_nearest(first, first_size, second, second_size, &sums);
    }
    if (status == STATUS_OK) {
        print_array("A", first, first_size, PRINT_LIMIT);
        print_array("B", second, second_size, PRINT_LIMIT);
        print_array("C", sums, first_size, PRINT_LIMIT);
    }
    free(first);
    free(second);
    free(sums);
    return status;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <a> <b>\n", argv[0]);
        return 1;
    }
    long long low = 0;
    long long high = 0;
    status_t status = parse_bound(argv[1], &low);
    if (status == STATUS_OK) {
        status = parse_bound(argv[2], &high);
    }
    if (status == STATUS_OK &&
        (low > high || low < -MAX_ABS_BOUND || high > MAX_ABS_BOUND)) {
        status = STATUS_INVALID_RANGE;
    }
    if (status == STATUS_OK) {
        srand((unsigned)time(NULL));
        printf("-- 9.1\n");
        status = run_fixed_array((int)low, (int)high);
    }
    if (status == STATUS_OK) {
        printf("-- 9.2\n");
        status = run_dynamic_arrays();
    }
    if (status != STATUS_OK) {
        fprintf(stderr, "Error: %s\n", status_message(status));
        return 1;
    }
    return 0;
}
