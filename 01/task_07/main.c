#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define INITIAL_LEXEME_CAPACITY 32
#define CODE_BUFFER_SIZE 16
#define TENTH 10
#define SECOND 2
#define FIFTH 5

typedef enum {
    OK = 0,
    ERR_ARGS_COUNT,
    ERR_FLAG,
    ERR_FILE_OPEN,
    ERR_SAME_FILE,
    ERR_MEMORY,
    ERR_WRITE,
    END_OF_FILE
} status_t;

static int is_separator(const int ch)
{
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

static status_t parse_flag(const char *arg, char *flag)
{
    if (arg == NULL || strlen(arg) != 2 || (arg[0] != '-' && arg[0] != '/'))
        return ERR_FLAG;
    *flag = arg[1];
    return OK;
}

static status_t read_lexeme(FILE *file, char **lexeme)
{
    size_t capacity = INITIAL_LEXEME_CAPACITY;
    size_t size = 0;
    char *buffer;
    int ch;

    do {
        ch = fgetc(file);
    } while (ch != EOF && is_separator(ch));
    if (ch == EOF)
        return END_OF_FILE;
    buffer = malloc(capacity);
    if (buffer == NULL)
        return ERR_MEMORY;
    while (ch != EOF && !is_separator(ch)) {
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
        ch = fgetc(file);
    }
    buffer[size] = '\0';
    *lexeme = buffer;
    return OK;
}

static status_t write_lexeme(FILE *file, const char *lexeme, int *is_first)
{
    if ((!*is_first && fputc(' ', file) == EOF) || fputs(lexeme, file) == EOF)
        return ERR_WRITE;
    *is_first = 0;
    return OK;
}

static void number_to_base(unsigned value, const unsigned base, char *buffer)
{
    char reversed[CODE_BUFFER_SIZE];
    size_t length = 0;

    do {
        reversed[length++] = (char)('0' + value % base);
        value /= base;
    } while (value > 0);
    for (size_t i = 0; i < length; i++)
        buffer[i] = reversed[length - 1 - i];
    buffer[length] = '\0';
}

static status_t copy_string(const char *lexeme, char **result)
{
    char *copy = malloc(strlen(lexeme) + 1);

    if (copy == NULL)
        return ERR_MEMORY;
    strcpy(copy, lexeme);
    *result = copy;
    return OK;
}

static status_t to_lowercase(const char *lexeme, char **result)
{
    char *copy = malloc(strlen(lexeme) + 1);

    if (copy == NULL)
        return ERR_MEMORY;
    for (size_t i = 0; lexeme[i] != '\0'; i++)
        copy[i] = (char)tolower((unsigned char)lexeme[i]);
    copy[strlen(lexeme)] = '\0';
    *result = copy;
    return OK;
}

static status_t to_ascii_codes(const char *lexeme, const unsigned base, char **result)
{
    char *text = malloc(strlen(lexeme) * (CODE_BUFFER_SIZE - 1) + 1);
    size_t size = 0;

    if (text == NULL)
        return ERR_MEMORY;
    for (size_t i = 0; lexeme[i] != '\0'; i++) {
        char code[CODE_BUFFER_SIZE];

        number_to_base((unsigned char)lexeme[i], base, code);
        strcpy(text + size, code);
        size += strlen(code);
    }
    text[size] = '\0';
    *result = text;
    return OK;
}

static status_t transform_lexeme(const char *lexeme, const unsigned long index, char **result)
{
    char *lower;
    status_t status;

    if (index % TENTH == 0) {
        status = to_lowercase(lexeme, &lower);
        if (status != OK)
            return status;
        status = to_ascii_codes(lower, 4, result);
        free(lower);
        return status;
    }
    if (index % SECOND == 0)
        return to_lowercase(lexeme, result);
    if (index % FIFTH == 0)
        return to_ascii_codes(lexeme, 8, result);
    return copy_string(lexeme, result);
}

static status_t interleave_files(FILE *first, FILE *second, FILE *output)
{
    FILE *sources[2] = {first, second};
    int ended[2] = {0, 0};
    int is_first = 1;
    int turn = 0;
    status_t status = OK;

    while (status == OK && !(ended[0] && ended[1])) {
        if (!ended[turn]) {
            char *lexeme;

            status = read_lexeme(sources[turn], &lexeme);
            if (status == OK) {
                status = write_lexeme(output, lexeme, &is_first);
                free(lexeme);
            } else if (status == END_OF_FILE) {
                ended[turn] = 1;
                status = OK;
            }
        }
        turn = 1 - turn;
    }
    if (status == OK && fputc('\n', output) == EOF)
        status = ERR_WRITE;
    return status;
}

static status_t convert_file(FILE *input, FILE *output)
{
    unsigned long index = 1;
    int is_first = 1;
    char *lexeme;
    status_t status;

    while ((status = read_lexeme(input, &lexeme)) == OK) {
        char *result;

        status = transform_lexeme(lexeme, index, &result);
        free(lexeme);
        if (status != OK)
            return status;
        status = write_lexeme(output, result, &is_first);
        free(result);
        if (status != OK)
            return status;
        index++;
    }
    if (status == END_OF_FILE && fputc('\n', output) == EOF)
        return ERR_WRITE;
    return status == END_OF_FILE ? OK : status;
}

static status_t run_interleave(const char *path_one, const char *path_two, const char *path_out)
{
    FILE *first;
    FILE *second;
    FILE *output;
    status_t status;

    if (strcmp(path_one, path_out) == 0 || strcmp(path_two, path_out) == 0)
        return ERR_SAME_FILE;
    first = fopen(path_one, "r");
    if (first == NULL)
        return ERR_FILE_OPEN;
    second = fopen(path_two, "r");
    if (second == NULL) {
        fclose(first);
        return ERR_FILE_OPEN;
    }
    output = fopen(path_out, "w");
    if (output == NULL) {
        fclose(first);
        fclose(second);
        return ERR_FILE_OPEN;
    }
    status = interleave_files(first, second, output);
    fclose(first);
    fclose(second);
    if (fclose(output) != 0 && status == OK)
        status = ERR_WRITE;
    return status;
}

static status_t run_convert(const char *path_in, const char *path_out)
{
    FILE *input;
    FILE *output;
    status_t status;

    if (strcmp(path_in, path_out) == 0)
        return ERR_SAME_FILE;
    input = fopen(path_in, "r");
    if (input == NULL)
        return ERR_FILE_OPEN;
    output = fopen(path_out, "w");
    if (output == NULL) {
        fclose(input);
        return ERR_FILE_OPEN;
    }
    status = convert_file(input, output);
    fclose(input);
    if (fclose(output) != 0 && status == OK)
        status = ERR_WRITE;
    return status;
}

static const char *status_message(const status_t status)
{
    switch (status) {
    case ERR_ARGS_COUNT: return "Wrong number of arguments for this flag";
    case ERR_FLAG: return "Unknown flag";
    case ERR_FILE_OPEN: return "Cannot open file";
    case ERR_SAME_FILE: return "Output file must differ from input files";
    case ERR_MEMORY: return "Memory allocation failed";
    case ERR_WRITE: return "Write error";
    default: return "";
    }
}

int main(int argc, char *argv[])
{
    char flag;
    status_t status;

    if (argc < 2) {
        fprintf(stderr, "Usage: program <flag> <files>\n");
        return 1;
    }
    status = parse_flag(argv[1], &flag);
    if (status == OK && flag == 'r')
        status = argc == 5 ? run_interleave(argv[2], argv[3], argv[4]) : ERR_ARGS_COUNT;
    else if (status == OK && flag == 'a')
        status = argc == 4 ? run_convert(argv[2], argv[3]) : ERR_ARGS_COUNT;
    else if (status == OK)
        status = ERR_FLAG;
    if (status != OK) {
        fprintf(stderr, "%s\n", status_message(status));
        return 1;
    }
    return 0;
}
