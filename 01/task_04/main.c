#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define INITIAL_LINE_CAPACITY 64
#define NUMBER_BUFFER_SIZE 32
#define OUTPUT_PREFIX "out_"

typedef enum {
    OK = 0,
    ERR_ARGS_COUNT,
    ERR_FLAG,
    ERR_INPUT_OPEN,
    ERR_OUTPUT_OPEN,
    ERR_SAME_FILE,
    ERR_MEMORY,
    ERR_WRITE,
    END_OF_FILE
} status_t;

static int is_latin_letter(const int ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

static int is_digit_char(const int ch)
{
    return ch >= '0' && ch <= '9';
}

static status_t parse_flag(const char *arg, char *action, int *custom_output)
{
    size_t length;

    if (arg == NULL || (arg[0] != '-' && arg[0] != '/'))
        return ERR_FLAG;
    length = strlen(arg);
    if (length == 3 && arg[1] == 'n') {
        *custom_output = 1;
        *action = arg[2];
    } else if (length == 2) {
        *custom_output = 0;
        *action = arg[1];
    } else {
        return ERR_FLAG;
    }
    if (strchr("disa", *action) == NULL)
        return ERR_FLAG;
    return OK;
}

static status_t build_default_output_path(const char *input_path, char **output_path)
{
    const char *name = input_path;
    size_t directory_length;
    char *result;

    for (const char *p = input_path; *p != '\0'; p++)
        if (*p == '/' || *p == '\\')
            name = p + 1;
    directory_length = (size_t)(name - input_path);
    result = malloc(strlen(input_path) + strlen(OUTPUT_PREFIX) + 1);
    if (result == NULL)
        return ERR_MEMORY;
    memcpy(result, input_path, directory_length);
    strcpy(result + directory_length, OUTPUT_PREFIX);
    strcat(result, name);
    *output_path = result;
    return OK;
}

static status_t read_line(FILE *file, char **line, size_t *length)
{
    size_t capacity = INITIAL_LINE_CAPACITY;
    size_t size = 0;
    char *buffer = malloc(capacity);
    int ch;

    if (buffer == NULL)
        return ERR_MEMORY;
    while ((ch = fgetc(file)) != EOF && ch != '\n') {
        if (size + 1 >= capacity) {
            char *bigger = realloc(buffer, capacity * 2);

            if (bigger == NULL) {
                free(buffer);
                return ERR_MEMORY;
            }
            buffer = bigger;
            capacity *= 2;
        }
        buffer[size++] = (char)ch;
    }
    if (ch == EOF && size == 0) {
        free(buffer);
        return END_OF_FILE;
    }
    buffer[size] = '\0';
    *line = buffer;
    *length = size;
    return OK;
}

static status_t remove_digits(const char *line, const size_t length, char **result)
{
    size_t size = 0;
    char *buffer = malloc(length + 1);

    if (buffer == NULL)
        return ERR_MEMORY;
    for (size_t i = 0; i < length; i++)
        if (!is_digit_char(line[i]))
            buffer[size++] = line[i];
    buffer[size] = '\0';
    *result = buffer;
    return OK;
}

static size_t count_letters(const char *line, const size_t length)
{
    size_t count = 0;

    for (size_t i = 0; i < length; i++)
        if (is_latin_letter(line[i]))
            count++;
    return count;
}

static size_t count_others(const char *line, const size_t length)
{
    size_t count = 0;

    for (size_t i = 0; i < length; i++)
        if (!is_latin_letter(line[i]) && !is_digit_char(line[i]) && line[i] != ' ')
            count++;
    return count;
}

static status_t replace_non_digits(const char *line, const size_t length, char **result)
{
    size_t size = 0;
    char *buffer = malloc(length * 2 + 1);

    if (buffer == NULL)
        return ERR_MEMORY;
    for (size_t i = 0; i < length; i++) {
        if (is_digit_char(line[i])) {
            buffer[size++] = line[i];
        } else {
            sprintf(buffer + size, "%02X", (unsigned)(unsigned char)line[i]);
            size += 2;
        }
    }
    buffer[size] = '\0';
    *result = buffer;
    return OK;
}

static status_t number_to_text(const size_t value, char **result)
{
    char *buffer = malloc(NUMBER_BUFFER_SIZE);

    if (buffer == NULL)
        return ERR_MEMORY;
    snprintf(buffer, NUMBER_BUFFER_SIZE, "%zu", value);
    *result = buffer;
    return OK;
}

static status_t transform_line(const char action, const char *line, const size_t length, char **result)
{
    switch (action) {
    case 'd': return remove_digits(line, length, result);
    case 'i': return number_to_text(count_letters(line, length), result);
    case 's': return number_to_text(count_others(line, length), result);
    default: return replace_non_digits(line, length, result);
    }
}

static status_t process_file(const char action, FILE *input, FILE *output)
{
    status_t status;
    char *line;
    size_t length;

    while ((status = read_line(input, &line, &length)) == OK) {
        char *result;

        status = transform_line(action, line, length, &result);
        free(line);
        if (status != OK)
            return status;
        if (fputs(result, output) == EOF || fputc('\n', output) == EOF)
            status = ERR_WRITE;
        free(result);
        if (status != OK)
            return status;
    }
    return status == END_OF_FILE ? OK : status;
}

static status_t run(const char action, const char *input_path, const char *output_path)
{
    FILE *input;
    FILE *output;
    status_t status;

    if (strcmp(input_path, output_path) == 0)
        return ERR_SAME_FILE;
    input = fopen(input_path, "r");
    if (input == NULL)
        return ERR_INPUT_OPEN;
    output = fopen(output_path, "w");
    if (output == NULL) {
        fclose(input);
        return ERR_OUTPUT_OPEN;
    }
    status = process_file(action, input, output);
    fclose(input);
    if (fclose(output) != 0 && status == OK)
        status = ERR_WRITE;
    return status;
}

static const char *status_message(const status_t status)
{
    switch (status) {
    case ERR_ARGS_COUNT: return "Usage: program <flag> <input> [output when flag has n]";
    case ERR_FLAG: return "Unknown flag";
    case ERR_INPUT_OPEN: return "Cannot open input file";
    case ERR_OUTPUT_OPEN: return "Cannot open output file";
    case ERR_SAME_FILE: return "Input and output files must differ";
    case ERR_MEMORY: return "Memory allocation failed";
    case ERR_WRITE: return "Write error";
    default: return "";
    }
}

int main(int argc, char *argv[])
{
    char action;
    int custom_output;
    char *default_path = NULL;
    const char *output_path;
    status_t status;

    if (argc < 3) {
        fprintf(stderr, "%s\n", status_message(ERR_ARGS_COUNT));
        return 1;
    }
    status = parse_flag(argv[1], &action, &custom_output);
    if (status == OK && argc != (custom_output ? 4 : 3))
        status = ERR_ARGS_COUNT;
    if (status == OK) {
        if (custom_output) {
            output_path = argv[3];
        } else {
            status = build_default_output_path(argv[2], &default_path);
            output_path = default_path;
        }
    }
    if (status == OK)
        status = run(action, argv[2], output_path);
    free(default_path);
    if (status != OK) {
        fprintf(stderr, "%s\n", status_message(status));
        return 1;
    }
    return 0;
}
