#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    STATUS_OK,
    STATUS_NULL_ARGUMENT,
    STATUS_NO_MEMORY,
    STATUS_INVALID_BASE,
    STATUS_INVALID_NUMBER
} status_t;

static int digit_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'Z') {
        return c - 'A' + 10;
    }
    return -1;
}

static char digit_char(int value)
{
    return (char)(value < 10 ? '0' + value : 'A' + value - 10);
}

static status_t validate_number(const char *number, int base)
{
    if (number == NULL) {
        return STATUS_NULL_ARGUMENT;
    }
    if (*number == '\0') {
        return STATUS_INVALID_NUMBER;
    }
    for (; *number != '\0'; number++) {
        int digit = digit_value(*number);
        if (digit < 0 || digit >= base) {
            return STATUS_INVALID_NUMBER;
        }
    }
    return STATUS_OK;
}

status_t add_in_columns(const char *left, const char *right, int base, char **result)
{
    if (result == NULL) {
        return STATUS_NULL_ARGUMENT;
    }
    if (base < 2 || base > 36) {
        return STATUS_INVALID_BASE;
    }
    status_t status = validate_number(left, base);
    if (status == STATUS_OK) {
        status = validate_number(right, base);
    }
    if (status != STATUS_OK) {
        return status;
    }
    size_t left_length = strlen(left);
    size_t right_length = strlen(right);
    size_t max_length = left_length > right_length ? left_length : right_length;
    char *digits = malloc(max_length + 2);
    if (digits == NULL) {
        return STATUS_NO_MEMORY;
    }
    int carry = 0;
    size_t length = 0;
    for (size_t i = 0; i < max_length; i++) {
        int left_digit = i < left_length ? digit_value(left[left_length - 1 - i]) : 0;
        int right_digit = i < right_length ? digit_value(right[right_length - 1 - i]) : 0;
        int sum = left_digit + right_digit + carry;
        digits[length++] = digit_char(sum % base);
        carry = sum / base;
    }
    if (carry > 0) {
        digits[length++] = digit_char(carry);
    }
    while (length > 1 && digits[length - 1] == '0') {
        length--;
    }
    for (size_t i = 0; i < length / 2; i++) {
        char tmp = digits[i];
        digits[i] = digits[length - 1 - i];
        digits[length - 1 - i] = tmp;
    }
    digits[length] = '\0';
    *result = digits;
    return STATUS_OK;
}

static char *copy_string(const char *text)
{
    char *copy = malloc(strlen(text) + 1);
    if (copy != NULL) {
        strcpy(copy, text);
    }
    return copy;
}

status_t sum_in_base(char **result, int base, size_t count, ...)
{
    if (result == NULL) {
        return STATUS_NULL_ARGUMENT;
    }
    if (base < 2 || base > 36) {
        return STATUS_INVALID_BASE;
    }
    char *total = copy_string("0");
    if (total == NULL) {
        return STATUS_NO_MEMORY;
    }
    status_t status = STATUS_OK;
    va_list args;
    va_start(args, count);
    for (size_t i = 0; i < count && status == STATUS_OK; i++) {
        const char *number = va_arg(args, const char *);
        char *next = NULL;
        status = add_in_columns(total, number, base, &next);
        if (status == STATUS_OK) {
            free(total);
            total = next;
        }
    }
    va_end(args);
    if (status != STATUS_OK) {
        free(total);
        return status;
    }
    *result = total;
    return STATUS_OK;
}

static const char *status_message(status_t status)
{
    switch (status) {
    case STATUS_NULL_ARGUMENT:
        return "null argument";
    case STATUS_NO_MEMORY:
        return "out of memory";
    case STATUS_INVALID_BASE:
        return "base must be in range 2..36";
    case STATUS_INVALID_NUMBER:
        return "invalid digit in number";
    default:
        return "unknown error";
    }
}

static void show(const char *title, status_t status, const char *value)
{
    if (status == STATUS_OK) {
        printf("%s = %s\n", title, value);
    } else {
        printf("%s: error: %s\n", title, status_message(status));
    }
}

int main(void)
{
    char *result = NULL;
    status_t status = sum_in_base(&result, 16, 3, "FF", "1", "a0");
    show("base 16: FF + 1 + a0", status, result);
    free(result);
    result = NULL;

    status = sum_in_base(&result, 2, 3, "1010", "0110", "1");
    show("base 2: 1010 + 0110 + 1", status, result);
    free(result);
    result = NULL;

    status = sum_in_base(&result, 10, 2, "99999999999999999999999999999999", "1");
    show("base 10: 99...9 (32 digits) + 1", status, result);
    free(result);
    result = NULL;

    status = sum_in_base(&result, 36, 2, "ZZZ", "1");
    show("base 36: ZZZ + 1", status, result);
    free(result);
    result = NULL;

    status = sum_in_base(&result, 10, 2, "000", "0000");
    show("base 10: 000 + 0000", status, result);
    free(result);
    result = NULL;

    status = sum_in_base(&result, 8, 2, "17", "8");
    show("base 8: 17 + 8", status, result);
    status = sum_in_base(&result, 40, 1, "1");
    show("base 40", status, result);
    return 0;
}
