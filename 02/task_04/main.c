#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NAME_LENGTH 255

typedef enum {
    STATUS_OK,
    STATUS_NULL_POINTER,
    STATUS_NO_MEMORY,
    STATUS_INVALID_INPUT
} Status;

typedef struct {
    char name[MAX_NAME_LENGTH + 1];
    long long size;
    long long importance;
} File_info;

typedef struct {
    File_info *files;
    size_t count;
    long long total_size;
    long long total_importance;
} Backup_result;

Status select_backup_files(const File_info *files, size_t count, long long capacity,
                           Backup_result *result)
{
    if (files == NULL || result == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (capacity < 0) {
        return STATUS_INVALID_INPUT;
    }
    result->files = NULL;
    result->count = 0;
    result->total_size = 0;
    result->total_importance = 0;

    size_t width = (size_t)capacity + 1;
    if (width == 0 || (count + 1) > SIZE_MAX / width / sizeof(long long)) {
        return STATUS_NO_MEMORY;
    }
    long long *table = calloc((count + 1) * width, sizeof *table);
    unsigned char *chosen = calloc(count, sizeof *chosen);
    if (table == NULL || chosen == NULL) {
        free(table);
        free(chosen);
        return STATUS_NO_MEMORY;
    }

    for (size_t i = 1; i <= count; i++) {
        const File_info *file = &files[i - 1];
        for (size_t weight = 0; weight < width; weight++) {
            long long best = table[(i - 1) * width + weight];
            if (file->size <= (long long)weight) {
                long long candidate = table[(i - 1) * width + weight - (size_t)file->size] + file->importance;
                if (candidate > best) {
                    best = candidate;
                }
            }
            table[i * width + weight] = best;
        }
    }

    size_t weight = (size_t)capacity;
    size_t chosen_count = 0;
    for (size_t i = count; i > 0; i--) {
        if (table[i * width + weight] != table[(i - 1) * width + weight]) {
            chosen[i - 1] = 1;
            weight -= (size_t)files[i - 1].size;
            chosen_count++;
        }
    }

    File_info *selected = NULL;
    if (chosen_count > 0) {
        selected = malloc(chosen_count * sizeof *selected);
        if (selected == NULL) {
            free(table);
            free(chosen);
            return STATUS_NO_MEMORY;
        }
    }
    size_t position = 0;
    for (size_t i = 0; i < count; i++) {
        if (chosen[i]) {
            selected[position++] = files[i];
            result->total_size += files[i].size;
            result->total_importance += files[i].importance;
        }
    }
    result->files = selected;
    result->count = chosen_count;
    free(table);
    free(chosen);
    return STATUS_OK;
}

Status read_input(FILE *input, File_info **files, size_t *count, long long *capacity)
{
    long long file_count = 0;
    if (fscanf(input, "%lld %lld", &file_count, capacity) != 2) {
        return STATUS_INVALID_INPUT;
    }
    if (file_count <= 0 || *capacity < 0) {
        return STATUS_INVALID_INPUT;
    }
    File_info *items = malloc((size_t)file_count * sizeof *items);
    if (items == NULL) {
        return STATUS_NO_MEMORY;
    }
    for (long long i = 0; i < file_count; i++) {
        File_info *item = &items[i];
        if (fscanf(input, "%255s %lld %lld", item->name, &item->size, &item->importance) != 3 ||
            item->size <= 0 || item->importance < 0) {
            free(items);
            return STATUS_INVALID_INPUT;
        }
        for (long long j = 0; j < i; j++) {
            if (strcmp(items[j].name, item->name) == 0) {
                free(items);
                return STATUS_INVALID_INPUT;
            }
        }
    }
    *files = items;
    *count = (size_t)file_count;
    return STATUS_OK;
}

void print_result(const Backup_result *result)
{
    printf("TOTAL_SIZE %lld\n", result->total_size);
    printf("TOTAL_IMPORTANCE %lld\n", result->total_importance);
    printf("FILES %zu\n", result->count);
    for (size_t i = 0; i < result->count; i++) {
        printf("%s\n", result->files[i].name);
    }
}

int main(void)
{
    File_info *files = NULL;
    size_t count = 0;
    long long capacity = 0;
    Status status = read_input(stdin, &files, &count, &capacity);
    if (status != STATUS_OK) {
        fprintf(stderr, "Error: %s\n", status == STATUS_NO_MEMORY ? "out of memory" : "invalid input");
        return 1;
    }
    Backup_result result;
    status = select_backup_files(files, count, capacity, &result);
    free(files);
    if (status != STATUS_OK) {
        fprintf(stderr, "Error: %s\n", status == STATUS_NO_MEMORY ? "out of memory" : "invalid input");
        return 1;
    }
    print_result(&result);
    free(result.files);
    return 0;
}
