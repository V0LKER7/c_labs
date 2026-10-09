#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    STATUS_OK,
    STATUS_NULL_POINTER,
    STATUS_INVALID_ARGUMENT,
    STATUS_NO_MEMORY,
    STATUS_FILE_ERROR
} Status;

typedef struct {
    size_t file_index;
    size_t line;
    size_t column;
} Match;

typedef struct {
    Match *items;
    size_t count;
    size_t capacity;
} Match_list;

void match_list_init(Match_list *list)
{
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void match_list_free(Match_list *list)
{
    free(list->items);
    match_list_init(list);
}

static Status match_list_add(Match_list *list, Match match)
{
    if (list->count == list->capacity) {
        size_t new_capacity = list->capacity == 0 ? 16 : list->capacity * 2;
        Match *temp = realloc(list->items, new_capacity * sizeof *temp);
        if (temp == NULL) {
            return STATUS_NO_MEMORY;
        }
        list->items = temp;
        list->capacity = new_capacity;
    }
    list->items[list->count++] = match;
    return STATUS_OK;
}

static size_t string_length(const char *text)
{
    size_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static int matches_at(const char *data, const char *substring, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        if (data[i] != substring[i]) {
            return 0;
        }
    }
    return 1;
}

static Status read_file(const char *path, char **data, size_t *size)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return STATUS_FILE_ERROR;
    }
    size_t capacity = 4096;
    size_t length = 0;
    char *buffer = malloc(capacity);
    if (buffer == NULL) {
        fclose(file);
        return STATUS_NO_MEMORY;
    }
    size_t read_count;
    while ((read_count = fread(buffer + length, 1, capacity - length, file)) > 0) {
        length += read_count;
        if (length == capacity) {
            char *temp = realloc(buffer, capacity * 2);
            if (temp == NULL) {
                free(buffer);
                fclose(file);
                return STATUS_NO_MEMORY;
            }
            buffer = temp;
            capacity *= 2;
        }
    }
    int failed = ferror(file);
    fclose(file);
    if (failed) {
        free(buffer);
        return STATUS_FILE_ERROR;
    }
    *data = buffer;
    *size = length;
    return STATUS_OK;
}

static Status search_buffer(const char *data, size_t size, const char *substring,
                            size_t substring_length, size_t file_index, Match_list *list)
{
    size_t line = 1;
    size_t column = 1;
    for (size_t i = 0; i < size; i++) {
        if (size - i >= substring_length && matches_at(data + i, substring, substring_length)) {
            Match match = {file_index, line, column};
            Status status = match_list_add(list, match);
            if (status != STATUS_OK) {
                return status;
            }
        }
        if (data[i] == '\n') {
            line++;
            column = 1;
        } else {
            column++;
        }
    }
    return STATUS_OK;
}

Status find_in_files(Match_list *list, const char *substring, size_t file_count, ...)
{
    if (list == NULL || substring == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (substring[0] == '\0') {
        return STATUS_INVALID_ARGUMENT;
    }
    size_t substring_length = string_length(substring);
    Status status = STATUS_OK;
    va_list args;
    va_start(args, file_count);
    for (size_t i = 0; i < file_count && status == STATUS_OK; i++) {
        const char *path = va_arg(args, const char *);
        if (path == NULL) {
            status = STATUS_NULL_POINTER;
            break;
        }
        char *data = NULL;
        size_t size = 0;
        status = read_file(path, &data, &size);
        if (status == STATUS_OK) {
            status = search_buffer(data, size, substring, substring_length, i, list);
            free(data);
        }
    }
    va_end(args);
    return status;
}

static void print_matches(const Match_list *list, const char *const *paths, size_t path_count)
{
    for (size_t i = 0; i < path_count; i++) {
        printf("File %s:\n", paths[i]);
        size_t found = 0;
        for (size_t j = 0; j < list->count; j++) {
            if (list->items[j].file_index == i) {
                printf("  line %zu, column %zu\n", list->items[j].line, list->items[j].column);
                found++;
            }
        }
        if (found == 0) {
            printf("  no matches\n");
        }
    }
}

static int write_sample(const char *path, const char *text)
{
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        return 0;
    }
    int ok = fputs(text, file) >= 0;
    return fclose(file) == 0 && ok;
}

static void run_search(const char *substring, const char *first, const char *second)
{
    const char *paths[] = {first, second};
    Match_list list;
    match_list_init(&list);
    printf("Substring: \"%s\"\n", substring);
    Status status = find_in_files(&list, substring, 2, first, second);
    if (status == STATUS_OK) {
        print_matches(&list, paths, 2);
    } else if (status == STATUS_FILE_ERROR) {
        printf("Error: cannot read one of the files\n");
    } else {
        printf("Error: status %d\n", status);
    }
    match_list_free(&list);
}

int main(void)
{
    const char *first = "sample_1.txt";
    const char *second = "sample_2.txt";
    if (!write_sample(first, "abc abc\nabcabc\n\tabc  abc") || !write_sample(second, "hello\nworld\nlo wo\nlololo")) {
        printf("Error: cannot create sample files\n");
        return 1;
    }
    run_search("abc", first, second);
    run_search("lo", first, second);
    run_search("lo\nwo", first, second);
    run_search("  ", first, second);
    run_search("zzz", first, second);
    run_search("abc", first, "missing.txt");
    remove(first);
    remove(second);
    return 0;
}
