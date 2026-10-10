# Задание 10: числа в системе счисления с основанием из консоли

## Условие

Пользователь вводит основание (2..36), затем строки с целыми числами в этой системе (цифры больше 9 заглавными латинскими буквами), ввод заканчивается строкой "Stop". Найти максимальное по модулю число и сумму всех чисел, напечатать их без ведущих нулей, а также в системах с основаниями 9, 18, 27, 36.

## Алгоритм

1. `read_line` читает строку через `fgets`, обрезает перевод строки, слишком длинная строка сообщает об ошибке и дочитывается до конца.
2. Основание проверяется функцией `parse_base`.
3. `parse_number`: допускает "-" в начале, проверяет каждую цифру (строчные буквы запрещены), накапливает модуль в `uint64_t` с проверкой, что он не больше `INT64_MAX`.
4. Сумма считается `add_checked` (проверка переполнения до сложения); максимум по модулю хранится вместе со знаком.
5. Некорректная строка или переполнение не прерывают программу: печатается "Skipped: ..." и ввод продолжается; конец ввода без "Stop" также завершает чтение.
6. `format_number` переводит значение в любую систему 2..36 и не создаёт ведущих нулей.

## Сборка

```
gcc -std=c99 -Wall -Wextra -pedantic -o task10 main.c
```

## Тесты

Обычный ввод (FF = 255, -1A0 = -416, 0010 = 16, сумма -145 = -91 в hex):

```
$ printf '16
FF
-1A0
0010
Stop
' | ./task10
Enter base (2..36): Enter number or Stop: Enter number or Stop: Enter number or Stop: Enter number or Stop: Maximum by module:
  base 16: -1A0 (input base)
  base  9: -512
  base 18: -152
  base 27: -FB
  base 36: -BK
Sum:
  base 16: -91 (input base)
  base  9: -171
  base 18: -81
  base 27: -5A
  base 36: -41
```

Некорректные строки пропускаются:

```
$ printf '2
101
102
abc
-11
Stop
' | ./task10
Enter base (2..36): Enter number or Stop: Enter number or Stop: Skipped: not a valid number in this base (digits above 9 must be uppercase)
Enter number or Stop: Skipped: not a valid number in this base (digits above 9 must be uppercase)
Enter number or Stop: Enter number or Stop: Maximum by module:
  base  2: 101 (input base)
  base  9: 5
  base 18: 5
  base 27: 5
  base 36: 5
Sum:
  base  2: 10 (input base)
  base  9: 2
  base 18: 2
  base 27: 2
  base 36: 2
```

Строчные буквы и переполнение:

```
$ printf '16
ff
FFFFFFFFFFFFFFFFFF
7FFFFFFFFFFFFFFF
1
Stop
' | ./task10
Enter base (2..36): Enter number or Stop: Skipped: not a valid number in this base (digits above 9 must be uppercase)
Enter number or Stop: Skipped: number or sum does not fit into 64 bits
Enter number or Stop: Enter number or Stop: Skipped: number or sum does not fit into 64 bits
Enter number or Stop: Maximum by module:
  base 16: 7FFFFFFFFFFFFFFF (input base)
  base  9: 67404283172107811827
  base 18: 16AGH595DF825FA7
  base 27: 27C48L5B37OAOP
  base 36: 1Y2P0IJ32E8E7
Sum:
  base 16: 7FFFFFFFFFFFFFFF (input base)
  base  9: 67404283172107811827
  base 18: 16AGH595DF825FA7
  base 27: 27C48L5B37OAOP
  base 36: 1Y2P0IJ32E8E7
```

Неверное основание:

```
$ printf '37
' | ./task10
Enter base (2..36): 
stderr: Error: base must be an integer from 2 to 36
```

Нет чисел:

```
$ printf '10
Stop
' | ./task10
Enter base (2..36): Enter number or Stop: No numbers were entered
```

