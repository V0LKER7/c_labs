#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    STATUS_OK,
    STATUS_NULL_ARGUMENT,
    STATUS_NO_MEMORY,
    STATUS_INVALID_NUMBER,
    STATUS_UNKNOWN_FLAG,
    STATUS_BAD_ARGUMENTS
} status_t;

static size_t str_len(const char *str)
{
    size_t length = 0;
    while (str[length] != '\0') {
        length++;
    }
    return length;
}

status_t calc_length(const char *str, size_t *length)
{
    if (str == NULL || length == NULL) {
        return STATUS_NULL_ARGUMENT;
    }
    *length = str_len(str);
    return STATUS_OK;
}

status_t reverse_string(const char *str, char **result)
{
    if (str == NULL || result == NULL) {
        return STATUS_NULL_ARGUMENT;
    }
    size_t length = str_len(str);
    char *output = malloc(length + 1);
    if (output == NULL) {
        return STATUS_NO_MEMORY;
    }
    for (size_t i = 0; i < length; i++) {
        output[i] = str[length - 1 - i];
    }
    output[length] = '\0';
    *result = output;
    return STATUS_OK;
}

status_t upper_odd_positions(const char *str, char **result)
{
    if (str == NULL || result == NULL) {
        return STATUS_NULL_ARGUMENT;
    }
    size_t length = str_len(str);
    char *output = malloc(length + 1);
    if (output == NULL) {
        return STATUS_NO_MEMORY;
    }
    for (size_t i = 0; i < length; i++) {
        output[i] = (i % 2 == 1) ? (char)toupper((unsigned char)str[i]) : str[i];
    }
    output[length] = '\0';
    *result = output;
    return STATUS_OK;
}

status_t sort_by_char_class(const char *str, char **result)
{
    if (str == NULL || result == NULL) {
        return STATUS_NULL_ARGUMENT;
    }
    size_t length = str_len(str);
    char *output = malloc(length + 1);
    if (output == NULL) {
        return STATUS_NO_MEMORY;
    }
    size_t position = 0;
    for (size_t i = 0; i < length; i++) {
        if (isdigit((unsigned char)str[i])) {
            output[position++] = str[i];
        }
    }
    for (size_t i = 0; i < length; i++) {
        if (isalpha((unsigned char)str[i])) {
            output[position++] = str[i];
        }
    }
    for (size_t i = 0; i < length; i++) {
        if (!isdigit((unsigned char)str[i]) && !isalpha((unsigned char)str[i])) {
            output[position++] = str[i];
        }
    }
    output[position] = '\0';
    *result = output;
    return STATUS_OK;
}

status_t concat_random_order(const char *const *strings, size_t count, unsigned int seed, char **result)
{
    if (strings == NULL || result == NULL) {
        return STATUS_NULL_ARGUMENT;
    }
    size_t total_length = 0;
    for (size_t i = 0; i < count; i++) {
        if (strings[i] == NULL) {
            return STATUS_NULL_ARGUMENT;
        }
        total_length += str_len(strings[i]);
    }
    size_t *order = malloc((count == 0 ? 1 : count) * sizeof(size_t));
    char *output = malloc(total_length + 1);
    if (order == NULL || output == NULL) {
        free(order);
        free(output);
        return STATUS_NO_MEMORY;
    }
    for (size_t i = 0; i < count; i++) {
        order[i] = i;
    }
    srand(seed);
    for (size_t i = count; i > 1; i--) {
        size_t j = (size_t)rand() % i;
        size_t tmp = order[i - 1];
        order[i - 1] = order[j];
        order[j] = tmp;
    }
    size_t position = 0;
    for (size_t i = 0; i < count; i++) {
        const char *part = strings[order[i]];
        for (size_t k = 0; part[k] != '\0'; k++) {
            output[position++] = part[k];
        }
    }
    output[position] = '\0';
    free(order);
    *result = output;
    return STATUS_OK;
}

static status_t parse_unsigned(const char *text, unsigned int *value)
{
    const unsigned int max_value = ~0u;
    if (text == NULL || value == NULL) {
        return STATUS_NULL_ARGUMENT;
    }
    if (*text == '\0') {
        return STATUS_INVALID_NUMBER;
    }
    unsigned int result = 0;
    for (; *text != '\0'; text++) {
        if (!isdigit((unsigned char)*text)) {
            return STATUS_INVALID_NUMBER;
        }
        unsigned int digit = (unsigned int)(*text - '0');
        if (result > (max_value - digit) / 10) {
            return STATUS_INVALID_NUMBER;
        }
        result = result * 10 + digit;
    }
    *value = result;
    return STATUS_OK;
}

static status_t run_concat(int argc, char *argv[], char **text)
{
    if (argc < 4) {
        return STATUS_BAD_ARGUMENTS;
    }
    unsigned int seed = 0;
    status_t status = parse_unsigned(argv[3], &seed);
    if (status != STATUS_OK) {
        return status;
    }
    size_t count = (size_t)(argc - 3);
    const char **strings = malloc(count * sizeof(const char *));
    if (strings == NULL) {
        return STATUS_NO_MEMORY;
    }
    strings[0] = argv[2];
    for (int i = 4; i < argc; i++) {
        strings[i - 3] = argv[i];
    }
    status = concat_random_order(strings, count, seed, text);
    free(strings);
    return status;
}

static status_t run_command(int argc, char *argv[], char **text, size_t *length, int *has_length)
{
    *text = NULL;
    *has_length = 0;
    if (argc < 3 || argv[1][0] != '-' || argv[1][1] == '\0' || argv[1][2] != '\0') {
        return STATUS_BAD_ARGUMENTS;
    }
    char flag = argv[1][1];
    if (flag == 'c') {
        return run_concat(argc, argv, text);
    }
    if (argc != 3) {
        return STATUS_BAD_ARGUMENTS;
    }
    switch (flag) {
    case 'l':
        *has_length = 1;
        return calc_length(argv[2], length);
    case 'r':
        return reverse_string(argv[2], text);
    case 'u':
        return upper_odd_positions(argv[2], text);
    case 'n':
        return sort_by_char_class(argv[2], text);
    default:
        return STATUS_UNKNOWN_FLAG;
    }
}

static const char *status_message(status_t status)
{
    switch (status) {
    case STATUS_NULL_ARGUMENT:
        return "null argument";
    case STATUS_NO_MEMORY:
        return "out of memory";
    case STATUS_INVALID_NUMBER:
        return "invalid unsigned number";
    case STATUS_UNKNOWN_FLAG:
        return "unknown flag";
    case STATUS_BAD_ARGUMENTS:
        return "usage: -l|-r|-u|-n <string> or -c <string> <seed> [strings...]";
    default:
        return "unknown error";
    }
}

int main(int argc, char *argv[])
{
    char *text = NULL;
    size_t length = 0;
    int has_length = 0;
    status_t status = run_command(argc, argv, &text, &length, &has_length);
    if (status != STATUS_OK) {
        fprintf(stderr, "Error: %s\n", status_message(status));
        return 1;
    }
    if (has_length) {
        printf("%zu\n", length);
    } else {
        printf("%s\n", text);
    }
    free(text);
    return 0;
}
