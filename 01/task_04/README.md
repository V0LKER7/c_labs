# Задание 4: обработка текстового файла

## Условие

Флаг и путь к входному файлу. Если после "-" или "/" стоит необязательная "n" (`-nd`, `/ni`, ...), путь к выходному файлу третий аргумент, иначе имя выходного файла это имя входного с префиксом "out_". Флаги:
- `-d` исключить символы арабских цифр
- `-i` для каждой строки записать количество латинских букв
- `-s` для каждой строки записать количество символов, не являющихся латинскими буквами, цифрами и пробелом
- `-a` заменить символы, не являющиеся цифрами, их ASCII-кодом в системе счисления 16

## Алгоритм

1. `parse_flag` определяет действие и наличие "n", затем проверяется количество аргументов (3 без "n", 4 с "n").
2. Без "n" имя выхода строится вставкой "out_" перед именем файла (после последнего "/" или "\\"), поэтому каталог входного файла сохраняется.
3. Файл читается построчно в динамический буфер (строки любой длины), строки обрабатываются чистыми функциями: `remove_digits`, `count_latin_letters`, `count_other_symbols`, `replace_non_digits_with_hex`; запись отдельно.
4. Для `-a` код записывается заглавными hex-цифрами без ведущих нулей, переводы строк сохраняются, цифры остаются как есть.
5. Совпадение путей входа и выхода, ошибки открытия, ошибки записи обрабатываются; файлы закрываются во всех ветках.

## Сборка

```
gcc -std=c99 -Wall -Wextra -pedantic -o task04 main.c
```

## Тесты

Входной файл `tests/sample.txt`:

```
abc123 XYZ!
Hello, World 42?

last 7
```

```
$ ./task04 -d sample.txt
[out_sample.txt]
abc XYZ!
Hello, World ?

last 
```

```
$ ./task04 -i sample.txt
[out_sample.txt]
6
10
0
4
```

```
$ ./task04 -s sample.txt
[out_sample.txt]
1
2
0
0
```

```
$ ./task04 /a sample.txt
[out_sample.txt]
6162631232058595A21
48656C6C6F2C20576F726C6420423F

6C617374207
```

```
$ ./task04 -nd sample.txt result.txt
[result.txt]
abc XYZ!
Hello, World ?

last 
```

```
$ ./task04 -d missing.txt
stderr: Error: cannot open input file
```

```
$ ./task04 -nd sample.txt
stderr: Error: wrong number of arguments
```

```
$ ./task04 -x sample.txt
stderr: Error: unknown flag
```

```
$ ./task04 -nd sample.txt sample.txt
stderr: Error: input and output paths must differ
```

