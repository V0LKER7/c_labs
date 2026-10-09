#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    STATUS_OK,
    STATUS_NULL_POINTER,
    STATUS_NO_MEMORY,
    STATUS_INVALID_ARGUMENT
} Status;

static size_t string_length(const char *text)
{
    size_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static char *create_string(size_t length)
{
    char *text = malloc(length + 1);
    if (text != NULL) {
        text[length] = '\0';
    }
    return text;
}

Status get_length(const char *source, size_t *length)
{
    if (source == NULL || length == NULL) {
        return STATUS_NULL_POINTER;
    }
    *length = string_length(source);
    return STATUS_OK;
}

Status reverse_string(const char *source, char **result)
{
    if (source == NULL || result == NULL) {
        return STATUS_NULL_POINTER;
    }
    size_t length = string_length(source);
    char *output = create_string(length);
    if (output == NULL) {
        return STATUS_NO_MEMORY;
    }
    for (size_t i = 0; i < length; i++) {
        output[i] = source[length - 1 - i];
    }
    *result = output;
    return STATUS_OK;
}

Status upper_odd_positions(const char *source, char **result)
{
    if (source == NULL || result == NULL) {
        return STATUS_NULL_POINTER;
    }
    size_t length = string_length(source);
    char *output = create_string(length);
    if (output == NULL) {
        return STATUS_NO_MEMORY;
    }
    for (size_t i = 0; i < length; i++) {
        output[i] = (i % 2 == 1) ? (char)toupper((unsigned char)source[i]) : source[i];
    }
    *result = output;
    return STATUS_OK;
}

Status group_characters(const char *source, char **result)
{
    if (source == NULL || result == NULL) {
        return STATUS_NULL_POINTER;
    }
    size_t length = string_length(source);
    char *output = create_string(length);
    if (output == NULL) {
        return STATUS_NO_MEMORY;
    }
    size_t position = 0;
    for (size_t i = 0; i < length; i++) {
        if (isdigit((unsigned char)source[i])) {
            output[position++] = source[i];
        }
    }
    for (size_t i = 0; i < length; i++) {
        if (isalpha((unsigned char)source[i])) {
            output[position++] = source[i];
        }
    }
    for (size_t i = 0; i < length; i++) {
        if (!isdigit((unsigned char)source[i]) && !isalpha((unsigned char)source[i])) {
            output[position++] = source[i];
        }
    }
    *result = output;
    return STATUS_OK;
}

Status concat_random(const char *const *strings, size_t count, unsigned int seed, char **result)
{
    if (strings == NULL || result == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (count == 0) {
        return STATUS_INVALID_ARGUMENT;
    }
    size_t *order = malloc(count * sizeof *order);
    if (order == NULL) {
        return STATUS_NO_MEMORY;
    }
    size_t total_length = 0;
    for (size_t i = 0; i < count; i++) {
        order[i] = i;
        total_length += string_length(strings[i]);
    }
    srand(seed);
    for (size_t i = count - 1; i > 0; i--) {
        size_t j = (size_t)rand() % (i + 1);
        size_t temp = order[i];
        order[i] = order[j];
        order[j] = temp;
    }
    char *output = create_string(total_length);
    if (output == NULL) {
        free(order);
        return STATUS_NO_MEMORY;
    }
    size_t position = 0;
    for (size_t i = 0; i < count; i++) {
        for (const char *symbol = strings[order[i]]; *symbol != '\0'; symbol++) {
            output[position++] = *symbol;
        }
    }
    free(order);
    *result = output;
    return STATUS_OK;
}

static Status parse_unsigned(const char *text, unsigned int *value)
{
    if (text[0] == '\0') {
        return STATUS_INVALID_ARGUMENT;
    }
    unsigned long long number = 0;
    for (const char *symbol = text; *symbol != '\0'; symbol++) {
        if (!isdigit((unsigned char)*symbol)) {
            return STATUS_INVALID_ARGUMENT;
        }
        number = number * 10 + (unsigned long long)(*symbol - '0');
        if (number > UINT_MAX) {
            return STATUS_INVALID_ARGUMENT;
        }
    }
    *value = (unsigned int)number;
    return STATUS_OK;
}

static void print_error(Status status)
{
    switch (status) {
        case STATUS_NULL_POINTER:
            fprintf(stderr, "Error: null pointer\n");
            break;
        case STATUS_NO_MEMORY:
            fprintf(stderr, "Error: out of memory\n");
            break;
        case STATUS_INVALID_ARGUMENT:
            fprintf(stderr, "Error: invalid argument\n");
            break;
        default:
            fprintf(stderr, "Error: unknown\n");
            break;
    }
}

static void print_usage(void)
{
    fprintf(stderr, "Usage:\n  prog -l|-r|-u|-n <string>\n  prog -c <string> <seed> [string ...]\n");
}

static int run_concat(int argc, char *argv[])
{
    if (argc < 4) {
        print_usage();
        return 1;
    }
    unsigned int seed = 0;
    Status status = parse_unsigned(argv[3], &seed);
    if (status != STATUS_OK) {
        print_error(status);
        return 1;
    }
    size_t count = (size_t)argc - 3;
    const char **strings = malloc(count * sizeof *strings);
    if (strings == NULL) {
        print_error(STATUS_NO_MEMORY);
        return 1;
    }
    strings[0] = argv[2];
    for (size_t i = 1; i < count; i++) {
        strings[i] = argv[3 + i];
    }
    char *result = NULL;
    status = concat_random(strings, count, seed, &result);
    free(strings);
    if (status != STATUS_OK) {
        print_error(status);
        return 1;
    }
    printf("%s\n", result);
    free(result);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 3 || argv[1][0] != '-' || argv[1][1] == '\0' || argv[1][2] != '\0') {
        print_usage();
        return 1;
    }
    char flag = argv[1][1];
    if (flag == 'c') {
        return run_concat(argc, argv);
    }
    if (argc != 3) {
        print_usage();
        return 1;
    }
    if (flag == 'l') {
        size_t length = 0;
        Status status = get_length(argv[2], &length);
        if (status != STATUS_OK) {
            print_error(status);
            return 1;
        }
        printf("%zu\n", length);
        return 0;
    }
    char *result = NULL;
    Status status;
    switch (flag) {
        case 'r':
            status = reverse_string(argv[2], &result);
            break;
        case 'u':
            status = upper_odd_positions(argv[2], &result);
            break;
        case 'n':
            status = group_characters(argv[2], &result);
            break;
        default:
            print_usage();
            return 1;
    }
    if (status != STATUS_OK) {
        print_error(status);
        return 1;
    }
    printf("%s\n", result);
    free(result);
    return 0;
}
