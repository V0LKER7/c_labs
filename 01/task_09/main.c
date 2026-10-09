#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

#define FIXED_SIZE 20
#define MIN_DYNAMIC_SIZE 10
#define MAX_DYNAMIC_SIZE 10000
#define MIN_VALUE (-1000)
#define MAX_VALUE 1000
#define PREVIEW_COUNT 8

typedef enum {
    OK = 0,
    ERR_ARGS_COUNT,
    ERR_NUMBER,
    ERR_RANGE,
    ERR_ARGUMENT,
    ERR_MEMORY
} status_t;

static status_t parse_int(const char *text, int *number)
{
    char *end;
    long value;

    if (text == NULL || *text == '\0')
        return ERR_NUMBER;
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || *end != '\0' || value < -1000000000L || value > 1000000000L)
        return ERR_NUMBER;
    *number = (int)value;
    return OK;
}

static int random_in_range(const int low, const int high)
{
    return low + rand() % (high - low + 1);
}

static void fill_array(int *array, const size_t size, const int low, const int high)
{
    for (size_t i = 0; i < size; i++)
        array[i] = random_in_range(low, high);
}

static status_t swap_min_and_max(int *array, const size_t size)
{
    size_t min_index = 0;
    size_t max_index = 0;
    int swap;

    if (array == NULL || size == 0)
        return ERR_ARGUMENT;
    for (size_t i = 1; i < size; i++) {
        if (array[i] < array[min_index])
            min_index = i;
        if (array[i] > array[max_index])
            max_index = i;
    }
    swap = array[min_index];
    array[min_index] = array[max_index];
    array[max_index] = swap;
    return OK;
}

static int closest_value(const int *array, const size_t size, const int target)
{
    int closest = array[0];

    for (size_t i = 1; i < size; i++) {
        long long current = (long long)array[i] - target;
        long long best = (long long)closest - target;

        if (llabs(current) < llabs(best))
            closest = array[i];
    }
    return closest;
}

static status_t build_sum_array(const int *a, const size_t a_size, const int *b, const size_t b_size, int **result)
{
    int *c;

    if (a == NULL || b == NULL || a_size == 0 || b_size == 0 || result == NULL)
        return ERR_ARGUMENT;
    c = malloc(a_size * sizeof(int));
    if (c == NULL)
        return ERR_MEMORY;
    for (size_t i = 0; i < a_size; i++)
        c[i] = a[i] + closest_value(b, b_size, a[i]);
    *result = c;
    return OK;
}

static void print_array(const char *name, const int *array, const size_t size, const size_t limit)
{
    printf("%s (%zu elements):", name, size);
    for (size_t i = 0; i < size && i < limit; i++)
        printf(" %d", array[i]);
    if (size > limit)
        printf(" ...");
    printf("\n");
}

static status_t run_fixed_array(const int low, const int high)
{
    int array[FIXED_SIZE];
    status_t status;

    fill_array(array, FIXED_SIZE, low, high);
    print_array("fixed array before", array, FIXED_SIZE, FIXED_SIZE);
    status = swap_min_and_max(array, FIXED_SIZE);
    if (status == OK)
        print_array("fixed array after ", array, FIXED_SIZE, FIXED_SIZE);
    return status;
}

static status_t run_dynamic_arrays(void)
{
    size_t a_size = (size_t)random_in_range(MIN_DYNAMIC_SIZE, MAX_DYNAMIC_SIZE);
    size_t b_size = (size_t)random_in_range(MIN_DYNAMIC_SIZE, MAX_DYNAMIC_SIZE);
    int *a = malloc(a_size * sizeof(int));
    int *b = malloc(b_size * sizeof(int));
    int *c = NULL;
    status_t status = OK;

    if (a == NULL || b == NULL)
        status = ERR_MEMORY;
    if (status == OK) {
        fill_array(a, a_size, MIN_VALUE, MAX_VALUE);
        fill_array(b, b_size, MIN_VALUE, MAX_VALUE);
        status = build_sum_array(a, a_size, b, b_size, &c);
    }
    if (status == OK) {
        print_array("A", a, a_size, PREVIEW_COUNT);
        print_array("B", b, b_size, PREVIEW_COUNT);
        print_array("C", c, a_size, PREVIEW_COUNT);
    }
    free(a);
    free(b);
    free(c);
    return status;
}

static const char *status_message(const status_t status)
{
    switch (status) {
    case ERR_ARGS_COUNT: return "Usage: program <a> <b>";
    case ERR_NUMBER: return "a and b must be integers";
    case ERR_RANGE: return "Need a <= b and b - a + 1 <= RAND_MAX";
    case ERR_ARGUMENT: return "Invalid argument";
    case ERR_MEMORY: return "Memory allocation failed";
    default: return "";
    }
}

int main(int argc, char *argv[])
{
    int low;
    int high;
    status_t status;

    if (argc != 3) {
        fprintf(stderr, "%s\n", status_message(ERR_ARGS_COUNT));
        return 1;
    }
    status = parse_int(argv[1], &low);
    if (status == OK)
        status = parse_int(argv[2], &high);
    if (status == OK && (low > high || (long long)high - low + 1 > RAND_MAX))
        status = ERR_RANGE;
    if (status == OK) {
        srand((unsigned)time(NULL));
        status = run_fixed_array(low, high);
    }
    if (status == OK)
        status = run_dynamic_arrays();
    if (status != OK) {
        fprintf(stderr, "%s\n", status_message(status));
        return 1;
    }
    return 0;
}
