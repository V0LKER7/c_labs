#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ULL_MAX (~0ULL)

typedef enum {
    STATUS_OK,
    STATUS_NULL_ARGUMENT,
    STATUS_NO_MEMORY,
    STATUS_INVALID_INPUT,
    STATUS_OVERFLOW
} status_t;

typedef struct {
    char *name;
    unsigned long long size;
    unsigned long long importance;
} file_info_t;

typedef struct {
    file_info_t *files;
    size_t count;
    unsigned long long total_size;
    unsigned long long total_importance;
} backup_result_t;

void free_result(backup_result_t *result)
{
    if (result == NULL) {
        return;
    }
    free(result->files);
    result->files = NULL;
    result->count = 0;
    result->total_size = 0;
    result->total_importance = 0;
}

static status_t build_result(const file_info_t *files, const unsigned char *chosen, size_t count,
                             backup_result_t *result)
{
    size_t picked = 0;
    for (size_t i = 0; i < count; i++) {
        picked += chosen[i];
    }
    if (picked == 0) {
        return STATUS_OK;
    }
    result->files = malloc(picked * sizeof(file_info_t));
    if (result->files == NULL) {
        return STATUS_NO_MEMORY;
    }
    for (size_t i = 0; i < count; i++) {
        if (chosen[i]) {
            result->files[result->count++] = files[i];
            result->total_size += files[i].size;
            result->total_importance += files[i].importance;
        }
    }
    return STATUS_OK;
}

static status_t fill_table(const file_info_t *files, size_t count, size_t capacity,
                           unsigned long long *best, unsigned char *take)
{
    size_t width = capacity + 1;
    for (size_t i = 0; i < count; i++) {
        if (files[i].size > capacity) {
            continue;
        }
        size_t size = (size_t)files[i].size;
        for (size_t w = capacity; w >= size; w--) {
            if (files[i].importance > ULL_MAX - best[w - size]) {
                return STATUS_OVERFLOW;
            }
            unsigned long long candidate = best[w - size] + files[i].importance;
            if (candidate > best[w]) {
                best[w] = candidate;
                take[i * width + w] = 1;
            }
        }
    }
    return STATUS_OK;
}

status_t select_files(const file_info_t *files, size_t count, unsigned long long capacity,
                      backup_result_t *result)
{
    if (files == NULL || result == NULL) {
        return STATUS_NULL_ARGUMENT;
    }
    result->files = NULL;
    result->count = 0;
    result->total_size = 0;
    result->total_importance = 0;
    if (capacity >= SIZE_MAX) {
        return STATUS_NO_MEMORY;
    }
    size_t cap = (size_t)capacity;
    size_t width = cap + 1;
    if (count > SIZE_MAX / width) {
        return STATUS_NO_MEMORY;
    }
    unsigned long long *best = calloc(width, sizeof(unsigned long long));
    unsigned char *take = calloc(count * width, 1);
    unsigned char *chosen = calloc(count, 1);
    if (best == NULL || take == NULL || chosen == NULL) {
        free(best);
        free(take);
        free(chosen);
        return STATUS_NO_MEMORY;
    }
    status_t status = fill_table(files, count, cap, best, take);
    if (status == STATUS_OK) {
        size_t w = cap;
        for (size_t i = count; i > 0; i--) {
            if (take[(i - 1) * width + w]) {
                chosen[i - 1] = 1;
                w -= (size_t)files[i - 1].size;
            }
        }
        status = build_result(files, chosen, count, result);
    }
    free(best);
    free(take);
    free(chosen);
    return status;
}

static status_t read_token(FILE *stream, char **token)
{
    int c = fgetc(stream);
    while (c != EOF && isspace(c)) {
        c = fgetc(stream);
    }
    if (c == EOF) {
        return STATUS_INVALID_INPUT;
    }
    size_t capacity = 16;
    size_t length = 0;
    char *buffer = malloc(capacity);
    if (buffer == NULL) {
        return STATUS_NO_MEMORY;
    }
    while (c != EOF && !isspace(c)) {
        if (length + 1 >= capacity) {
            char *tmp = realloc(buffer, capacity * 2);
            if (tmp == NULL) {
                free(buffer);
                return STATUS_NO_MEMORY;
            }
            buffer = tmp;
            capacity *= 2;
        }
        buffer[length++] = (char)c;
        c = fgetc(stream);
    }
    buffer[length] = '\0';
    *token = buffer;
    return STATUS_OK;
}

static status_t parse_ull(const char *text, unsigned long long *value)
{
    unsigned long long result = 0;
    if (*text == '\0') {
        return STATUS_INVALID_INPUT;
    }
    for (; *text != '\0'; text++) {
        if (!isdigit((unsigned char)*text)) {
            return STATUS_INVALID_INPUT;
        }
        unsigned long long digit = (unsigned long long)(*text - '0');
        if (result > (ULL_MAX - digit) / 10) {
            return STATUS_INVALID_INPUT;
        }
        result = result * 10 + digit;
    }
    *value = result;
    return STATUS_OK;
}

static status_t read_number(FILE *stream, unsigned long long *value)
{
    char *token = NULL;
    status_t status = read_token(stream, &token);
    if (status != STATUS_OK) {
        return status;
    }
    status = parse_ull(token, value);
    free(token);
    return status;
}

void free_files(file_info_t *files, size_t count)
{
    if (files == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        free(files[i].name);
    }
    free(files);
}

static int is_duplicate_name(const file_info_t *files, size_t count, const char *name)
{
    for (size_t i = 0; i < count; i++) {
        if (strcmp(files[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

static status_t read_file_info(FILE *stream, const file_info_t *previous, size_t previous_count,
                               file_info_t *info)
{
    status_t status = read_token(stream, &info->name);
    if (status != STATUS_OK) {
        return status;
    }
    if (is_duplicate_name(previous, previous_count, info->name)) {
        return STATUS_INVALID_INPUT;
    }
    status = read_number(stream, &info->size);
    if (status != STATUS_OK) {
        return status;
    }
    if (info->size == 0) {
        return STATUS_INVALID_INPUT;
    }
    return read_number(stream, &info->importance);
}

status_t read_input(FILE *stream, file_info_t **files, size_t *count, unsigned long long *capacity)
{
    unsigned long long declared_count = 0;
    status_t status = read_number(stream, &declared_count);
    if (status == STATUS_OK) {
        status = read_number(stream, capacity);
    }
    if (status != STATUS_OK) {
        return status;
    }
    if (declared_count == 0 || declared_count > SIZE_MAX) {
        return STATUS_INVALID_INPUT;
    }
    size_t n = (size_t)declared_count;
    file_info_t *array = calloc(n, sizeof(file_info_t));
    if (array == NULL) {
        return STATUS_NO_MEMORY;
    }
    for (size_t i = 0; i < n && status == STATUS_OK; i++) {
        status = read_file_info(stream, array, i, &array[i]);
    }
    if (status != STATUS_OK) {
        free_files(array, n);
        return status;
    }
    *files = array;
    *count = n;
    return STATUS_OK;
}

static void print_result(const backup_result_t *result)
{
    printf("TOTAL_SIZE %llu\n", result->total_size);
    printf("TOTAL_IMPORTANCE %llu\n", result->total_importance);
    printf("FILES %zu\n", result->count);
    for (size_t i = 0; i < result->count; i++) {
        printf("%s\n", result->files[i].name);
    }
}

static const char *status_message(status_t status)
{
    switch (status) {
    case STATUS_NULL_ARGUMENT:
        return "null argument";
    case STATUS_NO_MEMORY:
        return "out of memory";
    case STATUS_INVALID_INPUT:
        return "invalid input data";
    case STATUS_OVERFLOW:
        return "total importance overflow";
    default:
        return "unknown error";
    }
}

int main(void)
{
    file_info_t *files = NULL;
    size_t count = 0;
    unsigned long long capacity = 0;
    backup_result_t result = {NULL, 0, 0, 0};
    status_t status = read_input(stdin, &files, &count, &capacity);
    if (status == STATUS_OK) {
        status = select_files(files, count, capacity, &result);
    }
    if (status != STATUS_OK) {
        fprintf(stderr, "Error: %s\n", status_message(status));
    } else {
        print_result(&result);
    }
    free_result(&result);
    free_files(files, count);
    return status == STATUS_OK ? 0 : 1;
}
