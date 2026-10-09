#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NAME_SIZE 64
#define LINE_SIZE 512
#define SALARY_EPSILON 1e-9

typedef enum {
    STATUS_OK,
    STATUS_NULL_POINTER,
    STATUS_NO_MEMORY,
    STATUS_INVALID_FLAG,
    STATUS_INVALID_DATA,
    STATUS_FILE_ERROR
} Status;

typedef struct {
    unsigned int id;
    char name[NAME_SIZE];
    char surname[NAME_SIZE];
    double salary;
} Employee;

static bool is_latin_word(const char *text)
{
    if (text[0] == '\0') {
        return false;
    }
    for (const char *symbol = text; *symbol != '\0'; symbol++) {
        bool is_lower = *symbol >= 'a' && *symbol <= 'z';
        bool is_upper = *symbol >= 'A' && *symbol <= 'Z';
        if (!is_lower && !is_upper) {
            return false;
        }
    }
    return true;
}

static Status parse_unsigned(const char *text, unsigned int *value)
{
    if (text[0] == '\0') {
        return STATUS_INVALID_DATA;
    }
    unsigned long long number = 0;
    for (const char *symbol = text; *symbol != '\0'; symbol++) {
        if (!isdigit((unsigned char)*symbol)) {
            return STATUS_INVALID_DATA;
        }
        number = number * 10 + (unsigned long long)(*symbol - '0');
        if (number > UINT_MAX) {
            return STATUS_INVALID_DATA;
        }
    }
    *value = (unsigned int)number;
    return STATUS_OK;
}

static Status parse_employee(const char *line, Employee *employee)
{
    char id_text[32];
    char salary_text[64];
    int consumed = 0;
    int fields = sscanf(line, "%31s %63s %63s %63s %n", id_text, employee->name,
                        employee->surname, salary_text, &consumed);
    if (fields != 4 || line[consumed] != '\0') {
        return STATUS_INVALID_DATA;
    }
    if (parse_unsigned(id_text, &employee->id) != STATUS_OK) {
        return STATUS_INVALID_DATA;
    }
    if (!is_latin_word(employee->name) || !is_latin_word(employee->surname)) {
        return STATUS_INVALID_DATA;
    }
    char *end = NULL;
    employee->salary = strtod(salary_text, &end);
    if (*end != '\0' || employee->salary < 0.0 || employee->salary > DBL_MAX) {
        return STATUS_INVALID_DATA;
    }
    return STATUS_OK;
}

static bool is_blank_line(const char *line)
{
    return line[strspn(line, " \t\r\n")] == '\0';
}

Status read_employees(FILE *input, Employee **employees, size_t *count)
{
    if (input == NULL || employees == NULL || count == NULL) {
        return STATUS_NULL_POINTER;
    }
    Employee *items = NULL;
    size_t size = 0;
    size_t capacity = 0;
    char line[LINE_SIZE];
    while (fgets(line, sizeof line, input) != NULL) {
        size_t length = strlen(line);
        if (length == sizeof line - 1 && line[length - 1] != '\n') {
            free(items);
            return STATUS_INVALID_DATA;
        }
        if (is_blank_line(line)) {
            continue;
        }
        if (size == capacity) {
            size_t new_capacity = capacity == 0 ? 8 : capacity * 2;
            Employee *temp = realloc(items, new_capacity * sizeof *temp);
            if (temp == NULL) {
                free(items);
                return STATUS_NO_MEMORY;
            }
            items = temp;
            capacity = new_capacity;
        }
        Status status = parse_employee(line, &items[size]);
        if (status != STATUS_OK) {
            free(items);
            return status;
        }
        size++;
    }
    *employees = items;
    *count = size;
    return STATUS_OK;
}

static int compare_doubles(double left, double right, double epsilon)
{
    if (fabs(left - right) <= epsilon) {
        return 0;
    }
    return left < right ? -1 : 1;
}

static int compare_ascending(const void *left_ptr, const void *right_ptr)
{
    const Employee *left = left_ptr;
    const Employee *right = right_ptr;
    int result = compare_doubles(left->salary, right->salary, SALARY_EPSILON);
    if (result != 0) {
        return result;
    }
    result = strcmp(left->surname, right->surname);
    if (result != 0) {
        return result;
    }
    result = strcmp(left->name, right->name);
    if (result != 0) {
        return result;
    }
    if (left->id == right->id) {
        return 0;
    }
    return left->id < right->id ? -1 : 1;
}

static int compare_descending(const void *left, const void *right)
{
    return compare_ascending(right, left);
}

Status write_employees(FILE *output, const Employee *employees, size_t count)
{
    if (output == NULL || (employees == NULL && count > 0)) {
        return STATUS_NULL_POINTER;
    }
    for (size_t i = 0; i < count; i++) {
        if (fprintf(output, "%u %s %s %.2f\n", employees[i].id, employees[i].name,
                    employees[i].surname, employees[i].salary) < 0) {
            return STATUS_FILE_ERROR;
        }
    }
    return STATUS_OK;
}

static Status parse_flag(const char *flag, bool *ascending)
{
    if (flag[0] == '\0' || flag[1] == '\0' || flag[2] != '\0') {
        return STATUS_INVALID_FLAG;
    }
    if (flag[0] != '-' && flag[0] != '/') {
        return STATUS_INVALID_FLAG;
    }
    if (flag[1] == 'a') {
        *ascending = true;
        return STATUS_OK;
    }
    if (flag[1] == 'd') {
        *ascending = false;
        return STATUS_OK;
    }
    return STATUS_INVALID_FLAG;
}

static const char *status_message(Status status)
{
    switch (status) {
        case STATUS_NULL_POINTER:
            return "null pointer";
        case STATUS_NO_MEMORY:
            return "out of memory";
        case STATUS_INVALID_FLAG:
            return "invalid flag";
        case STATUS_INVALID_DATA:
            return "invalid data in input file";
        case STATUS_FILE_ERROR:
            return "file error";
        default:
            return "unknown error";
    }
}

int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <input> <-a|-d|/a|/d> <output>\n", argv[0]);
        return 1;
    }
    bool ascending = true;
    Status status = parse_flag(argv[2], &ascending);
    if (status != STATUS_OK) {
        fprintf(stderr, "Error: %s\n", status_message(status));
        return 1;
    }
    if (strcmp(argv[1], argv[3]) == 0) {
        fprintf(stderr, "Error: input and output files must differ\n");
        return 1;
    }
    FILE *input = fopen(argv[1], "r");
    if (input == NULL) {
        fprintf(stderr, "Error: cannot open input file\n");
        return 1;
    }
    Employee *employees = NULL;
    size_t count = 0;
    status = read_employees(input, &employees, &count);
    fclose(input);
    if (status != STATUS_OK) {
        fprintf(stderr, "Error: %s\n", status_message(status));
        return 1;
    }
    if (count > 1) {
        qsort(employees, count, sizeof *employees, ascending ? compare_ascending : compare_descending);
    }
    FILE *output = fopen(argv[3], "w");
    if (output == NULL) {
        fprintf(stderr, "Error: cannot open output file\n");
        free(employees);
        return 1;
    }
    status = write_employees(output, employees, count);
    if (fclose(output) != 0 && status == STATUS_OK) {
        status = STATUS_FILE_ERROR;
    }
    free(employees);
    if (status != STATUS_OK) {
        fprintf(stderr, "Error: %s\n", status_message(status));
        return 1;
    }
    return 0;
}
