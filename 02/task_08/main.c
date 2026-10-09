#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    STATUS_OK,
    STATUS_NULL_POINTER,
    STATUS_NO_MEMORY,
    STATUS_INVALID_BASE,
    STATUS_INVALID_NUMBER
} Status;

static const char *const DIGITS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static int digit_value(char symbol)
{
    if (symbol >= '0' && symbol <= '9') {
        return symbol - '0';
    }
    if (symbol >= 'a' && symbol <= 'z') {
        return symbol - 'a' + 10;
    }
    if (symbol >= 'A' && symbol <= 'Z') {
        return symbol - 'A' + 10;
    }
    return -1;
}

static Status validate_number(const char *text, int base)
{
    if (text == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (text[0] == '\0') {
        return STATUS_INVALID_NUMBER;
    }
    for (const char *symbol = text; *symbol != '\0'; symbol++) {
        int value = digit_value(*symbol);
        if (value < 0 || value >= base) {
            return STATUS_INVALID_NUMBER;
        }
    }
    return STATUS_OK;
}

Status add_in_columns(const char *left, const char *right, int base, char **result)
{
    if (result == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (base < 2 || base > 36) {
        return STATUS_INVALID_BASE;
    }
    Status status = validate_number(left, base);
    if (status != STATUS_OK) {
        return status;
    }
    status = validate_number(right, base);
    if (status != STATUS_OK) {
        return status;
    }
    size_t left_length = strlen(left);
    size_t right_length = strlen(right);
    size_t max_length = left_length > right_length ? left_length : right_length;
    char *reversed = malloc(max_length + 2);
    if (reversed == NULL) {
        return STATUS_NO_MEMORY;
    }
    int carry = 0;
    for (size_t i = 0; i < max_length; i++) {
        int a = i < left_length ? digit_value(left[left_length - 1 - i]) : 0;
        int b = i < right_length ? digit_value(right[right_length - 1 - i]) : 0;
        int sum = a + b + carry;
        reversed[i] = DIGITS[sum % base];
        carry = sum / base;
    }
    size_t length = max_length;
    if (carry > 0) {
        reversed[length++] = DIGITS[carry];
    }
    while (length > 1 && reversed[length - 1] == '0') {
        length--;
    }
    char *output = malloc(length + 1);
    if (output == NULL) {
        free(reversed);
        return STATUS_NO_MEMORY;
    }
    for (size_t i = 0; i < length; i++) {
        output[i] = reversed[length - 1 - i];
    }
    output[length] = '\0';
    free(reversed);
    *result = output;
    return STATUS_OK;
}

Status sum_in_base(int base, size_t count, char **result, ...)
{
    if (result == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (base < 2 || base > 36) {
        return STATUS_INVALID_BASE;
    }
    char *total = malloc(2);
    if (total == NULL) {
        return STATUS_NO_MEMORY;
    }
    total[0] = '0';
    total[1] = '\0';
    Status status = STATUS_OK;
    va_list args;
    va_start(args, result);
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

static void print_sum(const char *title, Status status, const char *sum)
{
    if (status == STATUS_OK) {
        printf("%s = %s\n", title, sum);
    } else {
        printf("%s: error %d\n", title, status);
    }
}

int main(void)
{
    char *sum = NULL;
    Status status = sum_in_base(16, 3, &sum, "FF", "1", "abc");
    print_sum("base 16: FF + 1 + abc", status, sum);
    free(sum);

    status = sum_in_base(2, 2, &sum, "0011", "0001");
    print_sum("base 2: 0011 + 0001", status, sum);
    free(sum);

    status = sum_in_base(10, 2, &sum, "99999999999999999999999999", "1");
    print_sum("base 10: 99999999999999999999999999 + 1", status, sum);
    free(sum);

    status = sum_in_base(36, 2, &sum, "Z", "1");
    print_sum("base 36: Z + 1", status, sum);
    free(sum);

    status = sum_in_base(10, 3, &sum, "000", "00", "0");
    print_sum("base 10: 000 + 00 + 0", status, sum);
    free(sum);

    status = sum_in_base(2, 2, &sum, "101", "102");
    print_sum("base 2: 101 + 102", status, NULL);

    status = sum_in_base(1, 1, &sum, "1");
    print_sum("base 1", status, NULL);
    return 0;
}
