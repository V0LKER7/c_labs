#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    STATUS_OK,
    STATUS_NULL_ARGUMENT,
    STATUS_NO_MEMORY,
    STATUS_OPEN_FILE,
    STATUS_READ_ERROR,
    STATUS_EMPTY_SUBSTRING,
    STATUS_BAD_ARGUMENTS
} status_t;

typedef struct {
    size_t line;
    size_t column;
} match_t;

typedef struct {
    const char *path;
    status_t status;
    match_t *matches;
    size_t count;
} file_result_t;

static size_t str_len(const char *str)
{
    size_t length = 0;
    while (str[length] != '\0') {
        length++;
    }
    return length;
}

static status_t read_file(const char *path, char **data, size_t *size)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return STATUS_OPEN_FILE;
    }
    size_t capacity = 4096;
    size_t length = 0;
    char *buffer = malloc(capacity);
    if (buffer == NULL) {
        fclose(file);
        return STATUS_NO_MEMORY;
    }
    size_t received = 0;
    do {
        if (length == capacity) {
            char *tmp = realloc(buffer, capacity * 2);
            if (tmp == NULL) {
                free(buffer);
                fclose(file);
                return STATUS_NO_MEMORY;
            }
            buffer = tmp;
            capacity *= 2;
        }
        received = fread(buffer + length, 1, capacity - length, file);
        length += received;
    } while (received > 0);
    int failed = ferror(file);
    fclose(file);
    if (failed) {
        free(buffer);
        return STATUS_READ_ERROR;
    }
    *data = buffer;
    *size = length;
    return STATUS_OK;
}

static int matches_at(const char *text, const char *substring, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        if (text[i] != substring[i]) {
            return 0;
        }
    }
    return 1;
}

static status_t append_match(match_t **matches, size_t *count, size_t *capacity, size_t line, size_t column)
{
    if (*count == *capacity) {
        size_t new_capacity = *capacity == 0 ? 8 : *capacity * 2;
        match_t *tmp = realloc(*matches, new_capacity * sizeof(match_t));
        if (tmp == NULL) {
            return STATUS_NO_MEMORY;
        }
        *matches = tmp;
        *capacity = new_capacity;
    }
    (*matches)[*count].line = line;
    (*matches)[*count].column = column;
    (*count)++;
    return STATUS_OK;
}

static status_t find_matches(const char *text, size_t text_size, const char *substring,
                             size_t substring_size, match_t **matches, size_t *count)
{
    match_t *found = NULL;
    size_t found_count = 0;
    size_t capacity = 0;
    size_t line = 1;
    size_t column = 1;
    for (size_t i = 0; i < text_size; i++) {
        if (text_size - i >= substring_size && matches_at(text + i, substring, substring_size)) {
            status_t status = append_match(&found, &found_count, &capacity, line, column);
            if (status != STATUS_OK) {
                free(found);
                return status;
            }
        }
        if (text[i] == '\n') {
            line++;
            column = 1;
        } else {
            column++;
        }
    }
    *matches = found;
    *count = found_count;
    return STATUS_OK;
}

static status_t search_file(const char *path, const char *substring, file_result_t *result)
{
    char *data = NULL;
    size_t size = 0;
    result->path = path;
    result->matches = NULL;
    result->count = 0;
    result->status = read_file(path, &data, &size);
    if (result->status == STATUS_NO_MEMORY) {
        return STATUS_NO_MEMORY;
    }
    if (result->status != STATUS_OK) {
        return STATUS_OK;
    }
    result->status = find_matches(data, size, substring, str_len(substring), &result->matches, &result->count);
    free(data);
    return result->status == STATUS_NO_MEMORY ? STATUS_NO_MEMORY : STATUS_OK;
}

void free_results(file_result_t *results, size_t count)
{
    if (results == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        free(results[i].matches);
    }
    free(results);
}

status_t find_in_files(file_result_t **results, const char *substring, size_t file_count, ...)
{
    if (results == NULL || substring == NULL) {
        return STATUS_NULL_ARGUMENT;
    }
    if (substring[0] == '\0') {
        return STATUS_EMPTY_SUBSTRING;
    }
    if (file_count == 0) {
        return STATUS_BAD_ARGUMENTS;
    }
    file_result_t *array = calloc(file_count, sizeof(file_result_t));
    if (array == NULL) {
        return STATUS_NO_MEMORY;
    }
    status_t status = STATUS_OK;
    va_list args;
    va_start(args, file_count);
    for (size_t i = 0; i < file_count && status == STATUS_OK; i++) {
        const char *path = va_arg(args, const char *);
        if (path == NULL) {
            status = STATUS_NULL_ARGUMENT;
        } else {
            status = search_file(path, substring, &array[i]);
        }
    }
    va_end(args);
    if (status != STATUS_OK) {
        free_results(array, file_count);
        return status;
    }
    *results = array;
    return STATUS_OK;
}

static const char *status_message(status_t status)
{
    switch (status) {
    case STATUS_NULL_ARGUMENT:
        return "null argument";
    case STATUS_NO_MEMORY:
        return "out of memory";
    case STATUS_OPEN_FILE:
        return "cannot open file";
    case STATUS_READ_ERROR:
        return "read error";
    case STATUS_EMPTY_SUBSTRING:
        return "substring must not be empty";
    case STATUS_BAD_ARGUMENTS:
        return "usage: <substring> <file1> [file2 ... file5]";
    default:
        return "unknown error";
    }
}

static void print_results(const file_result_t *results, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        printf("File: %s\n", results[i].path);
        if (results[i].status != STATUS_OK) {
            printf("  error: %s\n", status_message(results[i].status));
        } else if (results[i].count == 0) {
            printf("  no occurrences\n");
        }
        for (size_t k = 0; k < results[i].count; k++) {
            printf("  line %zu, column %zu\n", results[i].matches[k].line, results[i].matches[k].column);
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc < 3 || argc > 7) {
        fprintf(stderr, "Error: %s\n", status_message(STATUS_BAD_ARGUMENTS));
        return 1;
    }
    size_t file_count = (size_t)(argc - 2);
    file_result_t *results = NULL;
    status_t status = STATUS_BAD_ARGUMENTS;
    switch (file_count) {
    case 1:
        status = find_in_files(&results, argv[1], 1, argv[2]);
        break;
    case 2:
        status = find_in_files(&results, argv[1], 2, argv[2], argv[3]);
        break;
    case 3:
        status = find_in_files(&results, argv[1], 3, argv[2], argv[3], argv[4]);
        break;
    case 4:
        status = find_in_files(&results, argv[1], 4, argv[2], argv[3], argv[4], argv[5]);
        break;
    case 5:
        status = find_in_files(&results, argv[1], 5, argv[2], argv[3], argv[4], argv[5], argv[6]);
        break;
    default:
        break;
    }
    if (status != STATUS_OK) {
        fprintf(stderr, "Error: %s\n", status_message(status));
        return 1;
    }
    print_results(results, file_count);
    free_results(results, file_count);
    return 0;
}
