# Задание 3: флаги -q, -m, -t

## Условие

Флаг и набор чисел в аргументах командной строки, количество параметров проверяется:
- `-q eps a b c`: решения квадратного уравнения для всех уникальных перестановок коэффициентов
- `-m n1 n2`: кратно ли первое ненулевое целое число второму
- `-t eps s1 s2 s3`: могут ли числа быть сторонами прямоугольного треугольника

## Алгоритм

1. Флаг разбирается отдельной функцией, затем проверяется количество аргументов под этот флаг.
2. Числа разбираются через `strtod` и `strtoll` с проверкой `errno`, хвоста строки и конечности значения.
3. `-q`: шесть перестановок индексов, дубликаты (равные по eps тройки) отбрасываются. `solve_quadratic` возвращает структуру с типом решения: нет решений, любое x, один корень, два вещественных, пара комплексных. Если `|a| < eps`, уравнение линейное; дискриминант `|d| < eps` считается нулевым.
4. `-m`: оба числа ненулевые; для делителя -1 результат сразу "кратно" (защита от переполнения при `%`).
5. `-t`: стороны положительные; проверяется `|x^2 + y^2 - z^2| < eps` для трёх вариантов гипотенузы.

## Сборка

```
gcc -std=c99 -Wall -Wextra -pedantic -o task03 main.c -lm
```

## Тесты

```
$ ./task03 -q 1e-9 1 -3 2
a=1 b=-3 c=2: x1 = 1, x2 = 2
a=1 b=2 c=-3: x1 = -3, x2 = 1
a=-3 b=1 c=2: x1 = 1, x2 = -0.666667
a=-3 b=2 c=1: x1 = 1, x2 = -0.333333
a=2 b=1 c=-3: x1 = -1.5, x2 = 1
a=2 b=-3 c=1: x1 = 0.5, x2 = 1
```

```
$ ./task03 -q 1e-9 1 1 1
a=1 b=1 c=1: x1 = -0.5 + 0.866025i, x2 = -0.5 - 0.866025i
```

```
$ ./task03 -q 1e-9 0 0 1
a=0 b=0 c=1: no solutions
a=0 b=1 c=0: x = 0
a=1 b=0 c=0: x = 0
```

```
$ ./task03 -q 1e-9 1 2
stderr: Error: wrong number of arguments for this flag
```

```
$ ./task03 /m 12 4
12 is a multiple of 4
```

```
$ ./task03 -m 7 3
7 is not a multiple of 3
```

```
$ ./task03 -m 5 0
stderr: Error: value is not allowed for this flag
```

```
$ ./task03 -m 99999999999999999999 3
stderr: Error: argument is not a valid number
```

```
$ ./task03 -t 1e-9 3 4 5
sides 3 4 5 can form a right triangle
```

```
$ ./task03 -t 1e-9 5 3 4
sides 5 3 4 can form a right triangle
```

```
$ ./task03 -t 1e-9 2 3 4
sides 2 3 4 cannot form a right triangle
```

```
$ ./task03 -t 1e-9 -3 4 5
stderr: Error: value is not allowed for this flag
```

```
$ ./task03 -t 0 3 4 5
stderr: Error: epsilon must be a positive number
```

```
$ ./task03 -x 1
stderr: Error: unknown flag
```

