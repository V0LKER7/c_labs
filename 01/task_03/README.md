# Задание 3. Квадратное уравнение, кратность, треугольник

## Условие
Флаг и числа в аргументах. `-q eps a b c`: решения уравнения для всех уникальных перестановок коэффициентов;
`-m a b`: кратно ли a числу b (ненулевые целые); `-t eps a b c`: могут ли числа быть сторонами прямоугольного треугольника.

## Алгоритм
- Проверка флага и количества аргументов (`-q`, `-t`: 6, `-m`: 4), все числа валидируются через `strtod` / `strtoll`.
- `-q`: из 6 перестановок отбрасываются дубликаты (сравнение с eps). `solve_quadratic` возвращает структуру с видом решения и корнями: если a ~ 0 уравнение линейное, дискриминант сравнивается с 0 через eps.
- `-m`: `a % b == 0`, случай `b == -1` обработан отдельно.
- `-t`: стороны должны быть больше eps; проверка `|2*max^2 - (a^2 + b^2 + c^2)| < eps`, то есть `max^2 = сумме квадратов двух других`.

## Сборка
`gcc -std=c99 -Wall -Wextra -pedantic main.c -o prog -lm`

## Тесты
| Команда | Результат |
|---|---|
| `./prog -q 1e-9 1 -3 2` | 6 строк, например `a=1 b=-3 c=2: x1 = 1, x2 = 2` и `a=-3 b=1 c=2: x1 = 1, x2 = -0.666667` |
| `./prog -q 1e-9 2 2 -4` | 3 уникальные перестановки, в том числе `a=2 b=-4 c=2: x = 1` |
| `./prog -q 1e-9 1 1 1` | `a=1 b=1 c=1: no real roots` |
| `./prog -q 1e-9 0 0 0` | `a=0 b=0 c=0: any real number` |
| `./prog -m 10 5` | `10 is divisible by 5` |
| `./prog -m 7 3` | `7 is not divisible by 3` |
| `./prog -m -7 -1` | `-7 is divisible by -1` |
| `./prog -m 5 0` | ошибка `Numbers must be non-zero` |
| `./prog -t 1e-9 3 4 5` | `these can be sides of a right triangle` |
| `./prog /t 1e-9 5 3 4` | `these can be sides of a right triangle` |
| `./prog -t 1e-9 1 2 3` | `these cannot be sides of a right triangle` |
| `./prog -t 1e-9 -3 4 5` | ошибка `Sides must be positive` |
| `./prog -q 1e-9 1 2` | ошибка `Wrong number of arguments for this flag` |
| `./prog -q 0 1 2 3` | ошибка `eps must be a positive number` |
| `./prog -z` | ошибка `Unknown flag` |
