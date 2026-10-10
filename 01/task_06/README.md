# Задание 6: функции с переменным числом аргументов

## Условие

1. Выпуклость многоугольника по координатам вершин.
2. Значение многочлена степени n в точке (коэффициенты от старшей степени).
3. Числа Капрекара среди строковых представлений в системе счисления base.
4. Среднее геометрическое (количество чисел последний обязательный параметр).
5. Рекурсивное быстрое возведение вещественного числа в целую степень.
6. Корень уравнения методом дихотомии (указатель на функцию).

## Алгоритм

1. `is_convex_polygon(result, eps, count, ...)`: вершины читаются через `va_arg`, считаются векторные произведения соседних рёбер; выпуклый, если знаки одинаковые (с допуском eps) и суммарный угол поворота равен 2*pi (отсекает самопересекающиеся звёзды).
2. `evaluate_polynomial(result, x, degree, ...)`: схема Горнера, коэффициенты читаются слева направо от старшего.
3. `find_kaprekar_numbers(found, found_count, base, count, ...)`: строка переводится в число (с проверкой цифр и переполнения, число до 2^32 - 1), квадрат делится на `base^k`, ищется разбиение с положительной правой частью и суммой, равной числу (1 считается числом Капрекара).
4. `geometric_mean(result, count, ...)`: сумма логарифмов, затем `exp(sum / count)`; ноль даёт 0; отрицательные числа ошибка области определения.
5. `power_recursive`: `x^n = (x^(n/2))^2` (умножить на x при нечётном n), отрицательная степень через `1 / x^(-n)`, 0 в отрицательной степени и переполнение возвращают статус.
6. `find_root_bisection`: проверяется смена знака на концах, интервал делится пополам, пока длина больше eps.

## Сборка

```
gcc -std=c99 -Wall -Wextra -pedantic -o task06 main.c -lm
```

## Тесты

```
$ ./task06 
-- 6.1 convex polygon
square     convex
arrow      not convex
pentagram  not convex
two points error: invalid argument
-- 6.2 polynomial
x^3 - 2x^2 + 5 at x=2: 5
constant 7 at x=0.5: 7
negative degree: invalid argument
-- 6.3 kaprekar numbers
base 10 kaprekar: 1 9 45 55 297
base 16 kaprekar: FF 1
invalid digit: invalid number representation
-- 6.4 geometric mean
geometric mean of 1 3 9: 3
geometric mean of 4 9: 6
negative number: argument is outside of function domain
-- 6.5 fast power
2^10 = 1024
2^-3 = 0.125
1.5^7 = 17.0859
0^-1: argument is outside of function domain
10^1000: overflow
-- 6.6 bisection
x^2-2=0 on [0,2], eps=0.01: 1.4179687500
cos(x)-x=0 on [0,1], eps=0.01: 0.7382812500
x^3-x-2=0 on [1,2], eps=0.01: 1.5195312500
x^2-2=0 on [0,2], eps=1e-09: 1.4142135619
cos(x)-x=0 on [0,1], eps=1e-09: 0.7390851337
x^3-x-2=0 on [1,2], eps=1e-09: 1.5213797069
x^2-2=0 on [2,3]: root is not bracketed by the interval
```

