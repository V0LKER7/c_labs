#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_TOKEN_CAPACITY 32
#define MAX_DIGITS_PER_CODE 4

typedef enum {
    STATUS_OK,
    STATUS_INVALID_ARGS_COUNT,
    STATUS_INVALID_FLAG,
    STATUS_SAME_FILE,
    STATUS_OPEN_ERROR,
    STATUS_IO_ERROR,
    STATUS_MEMORY_ERROR,
    STATUS_END_OF_FILE
} status_t;

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} token_t;

static const char *status_message(status_t status)
{
    switch (status) {
    case STATUS_OK:
        return "ok";
    case STATUS_INVALID_ARGS_COUNT:
        return "wrong number of arguments for this flag";
    case STATUS_INVALID_FLAG:
        return "unknown flag";
    case STATUS_SAME_FILE:
        return "output path must differ from input paths";
    case STATUS_OPEN_ERROR:
        return "cannot open file";
    case STATUS_IO_ERROR:
        return "input/output error";
    case STATUS_MEMORY_ERROR:
        return "memory allocation failed";
    case STATUS_END_OF_FILE:
        return "end of file";
    }
    return "unknown error";
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

static int is_separator(int symbol)
{
    return symbol == ' ' || symbol == '\t' || symbol == '\n' || symbol == '\r';
}

static status_t ensure_capacity(char **buffer, size_t *capacity, size_t needed)
{
    if (needed <= *capacity) {
        return STATUS_OK;
    }
    size_t new_capacity = *capacity == 0 ? INITIAL_TOKEN_CAPACITY : *capacity;
    while (new_capacity < needed) {
        new_capacity *= 2;
    }
    char *grown = realloc(*buffer, new_capacity);
    if (grown == NULL) {
        return STATUS_MEMORY_ERROR;
    }
    *buffer = grown;
    *capacity = new_capacity;
    return STATUS_OK;
}

static status_t read_token(FILE *file, token_t *token)
{
    int symbol = fgetc(file);
    while (symbol != EOF && is_separator(symbol)) {
        symbol = fgetc(file);
    }
    if (symbol == EOF) {
        return ferror(file) ? STATUS_IO_ERROR : STATUS_END_OF_FILE;
    }
    token->length = 0;
    while (symbol != EOF && !is_separator(symbol)) {
        status_t status = ensure_capacity(&token->data, &token->capacity,
                                          token->length + 2);
        if (status != STATUS_OK) {
            return status;
        }
        token->data[token->length++] = (char)symbol;
        symbol = fgetc(file);
    }
    token->data[token->length] = '\0';
    return ferror(file) ? STATUS_IO_ERROR : STATUS_OK;
}

static status_t write_token(FILE *file, const char *text, size_t length,
                            int *is_first)
{
    if (!*is_first && fputc(' ', file) == EOF) {
        return STATUS_IO_ERROR;
    }
    *is_first = 0;
    if (fwrite(text, 1, length, file) != length) {
        return STATUS_IO_ERROR;
    }
    return STATUS_OK;
}

static status_t finish_output(FILE *file)
{
    return fputc('\n', file) == EOF ? STATUS_IO_ERROR : STATUS_OK;
}

static char to_lower_latin(char symbol)
{
    if (symbol >= 'A' && symbol <= 'Z') {
        return (char)(symbol - 'A' + 'a');
    }
    return symbol;
}

static size_t append_code(char *destination, unsigned char symbol, unsigned base)
{
    char reversed[MAX_DIGITS_PER_CODE];
    size_t length = 0;
    unsigned value = symbol;
    do {
        reversed[length++] = (char)('0' + value % base);
        value /= base;
    } while (value > 0);
    for (size_t index = 0; index < length; index++) {
        destination[index] = reversed[length - 1 - index];
    }
    return length;
}

static size_t transform_token(size_t position, const char *source, size_t length,
                              char *destination)
{
    size_t written = 0;
    for (size_t index = 0; index < length; index++) {
        char symbol = source[index];
        if (position % 10 == 0) {
            written += append_code(destination + written,
                                   (unsigned char)to_lower_latin(symbol), 4);
        } else if (position % 2 == 0) {
            destination[written++] = to_lower_latin(symbol);
        } else if (position % 5 == 0) {
            written += append_code(destination + written, (unsigned char)symbol, 8);
        } else {
            destination[written++] = symbol;
        }
    }
    destination[written] = '\0';
    return written;
}

static status_t read_next(FILE *file, token_t *token, int *is_open)
{
    status_t status = read_token(file, token);
    if (status == STATUS_END_OF_FILE) {
        *is_open = 0;
        return STATUS_OK;
    }
    return status;
}

static status_t merge_files(FILE *first, FILE *second, FILE *output)
{
    token_t token = {NULL, 0, 0};
    int first_open = 1;
    int second_open = 1;
    int is_first = 1;
    status_t status = STATUS_OK;
    while (status == STATUS_OK && (first_open || second_open)) {
        FILE *sources[2] = {first, second};
        int *states[2] = {&first_open, &second_open};
        for (int index = 0; index < 2 && status == STATUS_OK; index++) {
            if (!*states[index]) {
                continue;
            }
            status = read_next(sources[index], &token, states[index]);
            if (status == STATUS_OK && *states[index]) {
                status = write_token(output, token.data, token.length, &is_first);
            }
        }
    }
    free(token.data);
    return status == STATUS_OK ? finish_output(output) : status;
}

static status_t convert_file(FILE *input, FILE *output)
{
    token_t token = {NULL, 0, 0};
    char *work = NULL;
    size_t work_capacity = 0;
    size_t position = 0;
    int is_first = 1;
    status_t status = STATUS_OK;
    while (status == STATUS_OK) {
        status = read_token(input, &token);
        if (status != STATUS_OK) {
            break;
        }
        position++;
        status = ensure_capacity(&work, &work_capacity,
                                 token.length * MAX_DIGITS_PER_CODE + 1);
        if (status == STATUS_OK) {
            size_t length = transform_token(position, token.data, token.length, work);
            status = write_token(output, work, length, &is_first);
        }
    }
    free(token.data);
    free(work);
    if (status == STATUS_END_OF_FILE) {
        return finish_output(output);
    }
    return status;
}

static status_t close_output(FILE *output, status_t status)
{
    if (fclose(output) != 0 && status == STATUS_OK) {
        return STATUS_IO_ERROR;
    }
    return status;
}

static status_t run_merge(const char *first_path, const char *second_path,
                          const char *output_path)
{
    if (strcmp(output_path, first_path) == 0 || strcmp(output_path, second_path) == 0) {
        return STATUS_SAME_FILE;
    }
    FILE *first = fopen(first_path, "rb");
    if (first == NULL) {
        return STATUS_OPEN_ERROR;
    }
    FILE *second = fopen(second_path, "rb");
    if (second == NULL) {
        fclose(first);
        return STATUS_OPEN_ERROR;
    }
    FILE *output = fopen(output_path, "wb");
    if (output == NULL) {
        fclose(first);
        fclose(second);
        return STATUS_OPEN_ERROR;
    }
    status_t status = merge_files(first, second, output);
    fclose(first);
    fclose(second);
    return close_output(output, status);
}

static status_t run_convert(const char *input_path, const char *output_path)
{
    if (strcmp(input_path, output_path) == 0) {
        return STATUS_SAME_FILE;
    }
    FILE *input = fopen(input_path, "rb");
    if (input == NULL) {
        return STATUS_OPEN_ERROR;
    }
    FILE *output = fopen(output_path, "wb");
    if (output == NULL) {
        fclose(input);
        return STATUS_OPEN_ERROR;
    }
    status_t status = convert_file(input, output);
    fclose(input);
    return close_output(output, status);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s -r <file1> <file2> <output> | -a <input> <output>\n",
                argv[0]);
        return 1;
    }
    char flag = '\0';
    status_t status = parse_flag(argv[1], &flag);
    if (status == STATUS_OK) {
        if (flag == 'r') {
            status = argc == 5 ? run_merge(argv[2], argv[3], argv[4])
                               : STATUS_INVALID_ARGS_COUNT;
        } else if (flag == 'a') {
            status = argc == 4 ? run_convert(argv[2], argv[3])
                               : STATUS_INVALID_ARGS_COUNT;
        } else {
            status = STATUS_INVALID_FLAG;
        }
    }
    if (status != STATUS_OK) {
        fprintf(stderr, "Error: %s\n", status_message(status));
        return 1;
    }
    return 0;
}
