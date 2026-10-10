#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_LINE_CAPACITY 128
#define OUTPUT_PREFIX "out_"

typedef enum {
    STATUS_OK,
    STATUS_INVALID_ARGS_COUNT,
    STATUS_INVALID_FLAG,
    STATUS_SAME_FILE,
    STATUS_INPUT_OPEN_ERROR,
    STATUS_OUTPUT_OPEN_ERROR,
    STATUS_IO_ERROR,
    STATUS_MEMORY_ERROR,
    STATUS_END_OF_FILE
} status_t;

typedef struct {
    char action;
    int custom_output;
} options_t;

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
    int has_newline;
} line_t;

static const char *status_message(status_t status)
{
    switch (status) {
    case STATUS_OK:
        return "ok";
    case STATUS_INVALID_ARGS_COUNT:
        return "wrong number of arguments";
    case STATUS_INVALID_FLAG:
        return "unknown flag";
    case STATUS_SAME_FILE:
        return "input and output paths must differ";
    case STATUS_INPUT_OPEN_ERROR:
        return "cannot open input file";
    case STATUS_OUTPUT_OPEN_ERROR:
        return "cannot open output file";
    case STATUS_IO_ERROR:
        return "input/output error";
    case STATUS_MEMORY_ERROR:
        return "memory allocation failed";
    case STATUS_END_OF_FILE:
        return "end of file";
    }
    return "unknown error";
}

static status_t parse_flag(const char *argument, options_t *options)
{
    if (argument == NULL || options == NULL) {
        return STATUS_INVALID_FLAG;
    }
    if (argument[0] != '-' && argument[0] != '/') {
        return STATUS_INVALID_FLAG;
    }
    size_t position = 1;
    options->custom_output = 0;
    if (argument[position] == 'n') {
        options->custom_output = 1;
        position++;
    }
    char action = argument[position];
    if ((action != 'd' && action != 'i' && action != 's' && action != 'a') ||
        argument[position + 1] != '\0') {
        return STATUS_INVALID_FLAG;
    }
    options->action = action;
    return STATUS_OK;
}

static status_t build_output_path(const char *input_path, char **output_path)
{
    if (input_path == NULL || output_path == NULL) {
        return STATUS_INVALID_ARGS_COUNT;
    }
    size_t name_start = 0;
    for (size_t index = 0; input_path[index] != '\0'; index++) {
        if (input_path[index] == '/' || input_path[index] == '\\') {
            name_start = index + 1;
        }
    }
    size_t prefix_length = strlen(OUTPUT_PREFIX);
    size_t total = strlen(input_path) + prefix_length + 1;
    char *path = malloc(total);
    if (path == NULL) {
        return STATUS_MEMORY_ERROR;
    }
    memcpy(path, input_path, name_start);
    memcpy(path + name_start, OUTPUT_PREFIX, prefix_length);
    strcpy(path + name_start + prefix_length, input_path + name_start);
    *output_path = path;
    return STATUS_OK;
}

static int is_digit_symbol(unsigned char symbol)
{
    return symbol >= '0' && symbol <= '9';
}

static int is_latin_letter(unsigned char symbol)
{
    return (symbol >= 'a' && symbol <= 'z') || (symbol >= 'A' && symbol <= 'Z');
}

static void remove_digits(const char *line, size_t length, char *result,
                          size_t *result_length)
{
    size_t count = 0;
    for (size_t index = 0; index < length; index++) {
        if (!is_digit_symbol((unsigned char)line[index])) {
            result[count++] = line[index];
        }
    }
    result[count] = '\0';
    *result_length = count;
}

static size_t count_latin_letters(const char *line, size_t length)
{
    size_t count = 0;
    for (size_t index = 0; index < length; index++) {
        if (is_latin_letter((unsigned char)line[index])) {
            count++;
        }
    }
    return count;
}

static size_t count_other_symbols(const char *line, size_t length)
{
    size_t count = 0;
    for (size_t index = 0; index < length; index++) {
        unsigned char symbol = (unsigned char)line[index];
        if (!is_latin_letter(symbol) && !is_digit_symbol(symbol) &&
            symbol != ' ') {
            count++;
        }
    }
    return count;
}

static size_t append_hex_code(char *destination, unsigned char symbol)
{
    const char alphabet[] = "0123456789ABCDEF";
    if (symbol < 16) {
        destination[0] = alphabet[symbol];
        return 1;
    }
    destination[0] = alphabet[symbol / 16];
    destination[1] = alphabet[symbol % 16];
    return 2;
}

static void replace_non_digits_with_hex(const char *line, size_t length,
                                        char *result, size_t *result_length)
{
    size_t count = 0;
    for (size_t index = 0; index < length; index++) {
        unsigned char symbol = (unsigned char)line[index];
        if (is_digit_symbol(symbol)) {
            result[count++] = (char)symbol;
        } else {
            count += append_hex_code(result + count, symbol);
        }
    }
    result[count] = '\0';
    *result_length = count;
}

static status_t grow_line(line_t *line)
{
    size_t capacity = line->capacity == 0 ? INITIAL_LINE_CAPACITY : line->capacity * 2;
    char *data = realloc(line->data, capacity);
    if (data == NULL) {
        return STATUS_MEMORY_ERROR;
    }
    line->data = data;
    line->capacity = capacity;
    return STATUS_OK;
}

static status_t read_line(FILE *file, line_t *line)
{
    line->length = 0;
    line->has_newline = 0;
    int symbol = fgetc(file);
    if (symbol == EOF) {
        return ferror(file) ? STATUS_IO_ERROR : STATUS_END_OF_FILE;
    }
    while (symbol != EOF && symbol != '\n') {
        if (line->length + 1 >= line->capacity) {
            status_t status = grow_line(line);
            if (status != STATUS_OK) {
                return status;
            }
        }
        line->data[line->length++] = (char)symbol;
        symbol = fgetc(file);
    }
    if (line->capacity == 0) {
        status_t status = grow_line(line);
        if (status != STATUS_OK) {
            return status;
        }
    }
    line->data[line->length] = '\0';
    line->has_newline = (symbol == '\n');
    return ferror(file) ? STATUS_IO_ERROR : STATUS_OK;
}

static status_t write_text(FILE *file, const char *text, size_t length,
                           int newline)
{
    if (length > 0 && fwrite(text, 1, length, file) != length) {
        return STATUS_IO_ERROR;
    }
    if (newline && fputc('\n', file) == EOF) {
        return STATUS_IO_ERROR;
    }
    return STATUS_OK;
}

static status_t write_count(FILE *file, size_t count)
{
    return fprintf(file, "%zu\n", count) < 0 ? STATUS_IO_ERROR : STATUS_OK;
}

static status_t process_line(char action, const line_t *line, char **buffer,
                             size_t *buffer_capacity, FILE *output)
{
    if (action == 'i') {
        return write_count(output, count_latin_letters(line->data, line->length));
    }
    if (action == 's') {
        return write_count(output, count_other_symbols(line->data, line->length));
    }
    size_t needed = line->length * 2 + 1;
    if (needed > *buffer_capacity) {
        char *grown = realloc(*buffer, needed);
        if (grown == NULL) {
            return STATUS_MEMORY_ERROR;
        }
        *buffer = grown;
        *buffer_capacity = needed;
    }
    size_t result_length = 0;
    if (action == 'd') {
        remove_digits(line->data, line->length, *buffer, &result_length);
    } else {
        replace_non_digits_with_hex(line->data, line->length, *buffer,
                                    &result_length);
    }
    return write_text(output, *buffer, result_length, line->has_newline);
}

static status_t process_file(char action, FILE *input, FILE *output)
{
    line_t line = {NULL, 0, 0, 0};
    char *buffer = NULL;
    size_t buffer_capacity = 0;
    status_t status = STATUS_OK;
    while (status == STATUS_OK) {
        status = read_line(input, &line);
        if (status == STATUS_OK) {
            status = process_line(action, &line, &buffer, &buffer_capacity, output);
        }
    }
    free(line.data);
    free(buffer);
    return status == STATUS_END_OF_FILE ? STATUS_OK : status;
}

static status_t run(const options_t *options, const char *input_path,
                    const char *output_path)
{
    if (strcmp(input_path, output_path) == 0) {
        return STATUS_SAME_FILE;
    }
    FILE *input = fopen(input_path, "rb");
    if (input == NULL) {
        return STATUS_INPUT_OPEN_ERROR;
    }
    FILE *output = fopen(output_path, "wb");
    if (output == NULL) {
        fclose(input);
        return STATUS_OUTPUT_OPEN_ERROR;
    }
    status_t status = process_file(options->action, input, output);
    if (fclose(output) != 0 && status == STATUS_OK) {
        status = STATUS_IO_ERROR;
    }
    fclose(input);
    return status;
}

int main(int argc, char *argv[])
{
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Usage: %s <-d|-i|-s|-a> <input> or %s <-nd|-ni|-ns|-na> <input> <output>\n",
                argv[0], argv[0]);
        return 1;
    }
    options_t options = {'\0', 0};
    status_t status = parse_flag(argv[1], &options);
    if (status == STATUS_OK &&
        ((options.custom_output && argc != 4) || (!options.custom_output && argc != 3))) {
        status = STATUS_INVALID_ARGS_COUNT;
    }
    char *generated_path = NULL;
    const char *output_path = NULL;
    if (status == STATUS_OK) {
        if (options.custom_output) {
            output_path = argv[3];
        } else {
            status = build_output_path(argv[2], &generated_path);
            output_path = generated_path;
        }
    }
    if (status == STATUS_OK) {
        status = run(&options, argv[2], output_path);
    }
    free(generated_path);
    if (status != STATUS_OK) {
        fprintf(stderr, "Error: %s\n", status_message(status));
        return 1;
    }
    return 0;
}
