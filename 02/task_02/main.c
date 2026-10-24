#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NAME_SIZE 64
#define LINE_SIZE 512
#define SALARY_EPSILON 1e-9

typedef enum {
    STATUS_OK,
    STATUS_NULL_ARGUMENT,
    STATUS_NO_MEMORY,
    STATUS_OPEN_INPUT,
    STATUS_OPEN_OUTPUT,
    STATUS_BAD_FORMAT,
    STATUS_LINE_TOO_LONG,
    STATUS_BAD_FLAG,
    STATUS_WRITE_ERROR,
    STATUS_BAD_ARGUMENTS
} status_t;

typedef struct Employee {
    unsigned int id;
    char name[NAME_SIZE];
    char surname[NAME_SIZE];
    double salary;
} Employee;

static int compare_double(double a, double b, double eps)
{
    if (fabs(a - b) <= eps) {
        return 0;
    }
    return a < b ? -1 : 1;
}

static int is_blank(const char *line)
{
    for (; *line != '\0'; line++) {
        if (!isspace((unsigned char)*line)) {
            return 0;
        }
    }
    return 1;
}

static int is_latin_word(const char *text)
{
    if (*text == '\0') {
        return 0;
    }
    for (; *text != '\0'; text++) {
        int is_lower = *text >= 'a' && *text <= 'z';
        int is_upper = *text >= 'A' && *text <= 'Z';
        if (!is_lower && !is_upper) {
            return 0;
        }
    }
    return 1;
}

static status_t parse_id(const char *text, unsigned int *id)
{
    const unsigned int max_value = ~0u;
    unsigned int result = 0;
    if (*text == '\0') {
        return STATUS_BAD_FORMAT;
    }
    for (; *text != '\0'; text++) {
        if (!isdigit((unsigned char)*text)) {
            return STATUS_BAD_FORMAT;
        }
        unsigned int digit = (unsigned int)(*text - '0');
        if (result > (max_value - digit) / 10) {
            return STATUS_BAD_FORMAT;
        }
        result = result * 10 + digit;
    }
    *id = result;
    return STATUS_OK;
}

static status_t parse_salary(const char *text, double eps, double *salary)
{
    char *end = NULL;
    double value = strtod(text, &end);
    if (end == text || *end != '\0' || !isfinite(value)) {
        return STATUS_BAD_FORMAT;
    }
    if (compare_double(value, 0.0, eps) < 0) {
        return STATUS_BAD_FORMAT;
    }
    *salary = value;
    return STATUS_OK;
}

static status_t parse_employee(const char *line, double eps, Employee *employee)
{
    char id_text[32];
    char name[NAME_SIZE];
    char surname[NAME_SIZE];
    char salary_text[64];
    char extra[2];
    if (sscanf(line, "%31s %63s %63s %63s %1s", id_text, name, surname, salary_text, extra) != 4) {
        return STATUS_BAD_FORMAT;
    }
    if (!is_latin_word(name) || !is_latin_word(surname)) {
        return STATUS_BAD_FORMAT;
    }
    status_t status = parse_id(id_text, &employee->id);
    if (status != STATUS_OK) {
        return status;
    }
    status = parse_salary(salary_text, eps, &employee->salary);
    if (status != STATUS_OK) {
        return status;
    }
    strcpy(employee->name, name);
    strcpy(employee->surname, surname);
    return STATUS_OK;
}

static status_t append_employee(Employee **array, size_t *count, size_t *capacity, const Employee *employee)
{
    if (*count == *capacity) {
        size_t new_capacity = *capacity == 0 ? 8 : *capacity * 2;
        Employee *tmp = realloc(*array, new_capacity * sizeof(Employee));
        if (tmp == NULL) {
            return STATUS_NO_MEMORY;
        }
        *array = tmp;
        *capacity = new_capacity;
    }
    (*array)[*count] = *employee;
    (*count)++;
    return STATUS_OK;
}

status_t read_employees(const char *path, Employee **employees, size_t *count)
{
    if (path == NULL || employees == NULL || count == NULL) {
        return STATUS_NULL_ARGUMENT;
    }
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return STATUS_OPEN_INPUT;
    }
    Employee *array = NULL;
    size_t size = 0;
    size_t capacity = 0;
    status_t status = STATUS_OK;
    char line[LINE_SIZE];
    while (status == STATUS_OK && fgets(line, sizeof(line), file) != NULL) {
        if (strchr(line, '\n') == NULL && !feof(file)) {
            status = STATUS_LINE_TOO_LONG;
        } else if (!is_blank(line)) {
            Employee employee;
            status = parse_employee(line, SALARY_EPSILON, &employee);
            if (status == STATUS_OK) {
                status = append_employee(&array, &size, &capacity, &employee);
            }
        }
    }
    fclose(file);
    if (status != STATUS_OK) {
        free(array);
        return status;
    }
    *employees = array;
    *count = size;
    return STATUS_OK;
}

static int compare_employees_asc(const void *left, const void *right)
{
    const Employee *a = left;
    const Employee *b = right;
    int result = compare_double(a->salary, b->salary, SALARY_EPSILON);
    if (result != 0) {
        return result;
    }
    result = strcmp(a->surname, b->surname);
    if (result != 0) {
        return result;
    }
    result = strcmp(a->name, b->name);
    if (result != 0) {
        return result;
    }
    return (a->id > b->id) - (a->id < b->id);
}

static int compare_employees_desc(const void *left, const void *right)
{
    return compare_employees_asc(right, left);
}

status_t sort_employees(Employee *employees, size_t count, int ascending)
{
    if (employees == NULL && count > 0) {
        return STATUS_NULL_ARGUMENT;
    }
    if (count > 1) {
        qsort(employees, count, sizeof(Employee), ascending ? compare_employees_asc : compare_employees_desc);
    }
    return STATUS_OK;
}

status_t write_employees(const char *path, const Employee *employees, size_t count)
{
    if (path == NULL || (employees == NULL && count > 0)) {
        return STATUS_NULL_ARGUMENT;
    }
    FILE *file = fopen(path, "w");
    if (file == NULL) {
        return STATUS_OPEN_OUTPUT;
    }
    status_t status = STATUS_OK;
    for (size_t i = 0; i < count && status == STATUS_OK; i++) {
        if (fprintf(file, "%u %s %s %.2f\n", employees[i].id, employees[i].name,
                    employees[i].surname, employees[i].salary) < 0) {
            status = STATUS_WRITE_ERROR;
        }
    }
    if (fclose(file) != 0 && status == STATUS_OK) {
        status = STATUS_WRITE_ERROR;
    }
    return status;
}

static status_t parse_flag(const char *flag, int *ascending)
{
    if (flag[0] == '\0' || flag[1] == '\0' || flag[2] != '\0') {
        return STATUS_BAD_FLAG;
    }
    if (flag[0] != '-' && flag[0] != '/') {
        return STATUS_BAD_FLAG;
    }
    if (flag[1] == 'a') {
        *ascending = 1;
        return STATUS_OK;
    }
    if (flag[1] == 'd') {
        *ascending = 0;
        return STATUS_OK;
    }
    return STATUS_BAD_FLAG;
}

static const char *status_message(status_t status)
{
    switch (status) {
    case STATUS_NULL_ARGUMENT:
        return "null argument";
    case STATUS_NO_MEMORY:
        return "out of memory";
    case STATUS_OPEN_INPUT:
        return "cannot open input file";
    case STATUS_OPEN_OUTPUT:
        return "cannot open output file";
    case STATUS_BAD_FORMAT:
        return "invalid data in input file";
    case STATUS_LINE_TOO_LONG:
        return "line in input file is too long";
    case STATUS_BAD_FLAG:
        return "flag must be -a, -d, /a or /d";
    case STATUS_WRITE_ERROR:
        return "write error";
    case STATUS_BAD_ARGUMENTS:
        return "usage: <input> <flag> <output>";
    default:
        return "unknown error";
    }
}

int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "Error: %s\n", status_message(STATUS_BAD_ARGUMENTS));
        return 1;
    }
    int ascending = 1;
    status_t status = parse_flag(argv[2], &ascending);
    Employee *employees = NULL;
    size_t count = 0;
    if (status == STATUS_OK) {
        status = read_employees(argv[1], &employees, &count);
    }
    if (status == STATUS_OK) {
        status = sort_employees(employees, count, ascending);
    }
    if (status == STATUS_OK) {
        status = write_employees(argv[3], employees, count);
    }
    free(employees);
    if (status != STATUS_OK) {
        fprintf(stderr, "Error: %s\n", status_message(status));
        return 1;
    }
    return 0;
}
