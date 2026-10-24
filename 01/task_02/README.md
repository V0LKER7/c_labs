# Задание 2: константы e, pi, ln2, sqrt2, gamma

## Условие

Реализовать вычисление e, π, ln 2, √2, γ с точностью eps (аргумент командной строки) тремя способами: как предел, как сумма ряда (или произведение), как решение уравнения. Продемонстрировать работу.

## Алгоритм

1. eps проверяется: число из интервала (0, 1).
2. Пределы e, π, ln 2: n удваивается, пока |f(2n) - f(n)| не станет меньше eps. Для π используется рекуррентная форма `t_n = t_(n-1) * 2n / (2n-1)`, `f(n) = t_n^2 / n`, чтобы не считать факториалы. Для ln 2 используется `n * expm1(ln2 / n)`.
3. Ряды: слагаемые добавляются, пока очередное не станет меньше eps (e: 1/n!, π: 4 * сумма ряда Лейбница, ln 2: чередующийся ряд).
4. Уравнения решаются бисекцией: `ln x = 1` на [1,3]; `cos x = -1` решается как `cos(x/2) = 0` на [3,4], так как у `cos x + 1` нет смены знака; `e^x = 2` на [0,1]; `x^2 = 2` на [1,2]; для γ уравнение `e^(-x) = L`, где `L = ln t * произведение (p-1)/p` по простым p до t (решето Эратосфена, t удваивается до сходимости).
5. √2: предел рекуррентности `x(n+1) = x(n) - x(n)^2/2 + 1`, `x0 = -0.5`; произведение `2^(1/4) * 2^(1/8) * ...` до множителя, отличающегося от 1 меньше чем на eps.
6. γ: предел `S_m` считается в `long double`, критерий `|S_m - S_(m-1)| * m < eps` (предел сходится как 1/m, поэтому m ограничено 30). Ряд суммируется по блокам `k` от `m^2` до `(m+1)^2 - 1` с оценкой хвоста `2/M + 1/(6 M^2)`.
7. Если метод не достигает eps (лимит итераций, ограничения double), печатается "no convergence for this epsilon".

## Сборка

```
gcc -std=c99 -Wall -Wextra -pedantic -o task02 main.c -lm
```

## Тесты

```
$ ./task02 0.001
e      limit     2.717618482337
e      series    2.718253968254
e      equation  2.718261718750
pi     limit     3.142359737588
pi     series    3.142092403684
pi     equation  3.141113281250
ln2    limit     0.694086412852
ln2    series    0.693646431559
ln2    equation  0.692871093750
sqrt2  limit     1.414418716807
sqrt2  product   1.413256602950
sqrt2  equation  1.414550781250
gamma  limit     no convergence for this epsilon
gamma  series    0.577097477188
gamma  equation  0.578216552734
```

```
$ ./task02 0.000001
e      limit     2.718281180370
e      series    2.718281801146
e      equation  2.718282222748
pi     limit     3.141593402603
pi     series    3.141593153589
pi     equation  3.141592502594
ln2    limit     0.693148096952
ln2    series    0.693147680559
ln2    equation  0.693147182465
sqrt2  limit     1.414213740181
sqrt2  product   1.414212627526
sqrt2  equation  1.414213657379
gamma  limit     no convergence for this epsilon
gamma  series    0.577215504101
gamma  equation  no convergence for this epsilon
```

```
$ ./task02 0.00000001
e      limit     2.718281823396
e      series    2.718281828286
e      equation  2.718281831592
pi     limit     3.141592659436
pi     series    no convergence for this epsilon
pi     equation  3.141592655331
ln2    limit     0.693147187719
ln2    series    0.693147185560
ln2    equation  0.693147178739
sqrt2  limit     1.414213560205
sqrt2  product   1.414213555070
sqrt2  equation  1.414213564247
gamma  limit     no convergence for this epsilon
gamma  series    0.577215663246
gamma  equation  no convergence for this epsilon
```

```
$ ./task02 0
stderr: Error: epsilon must be a real number in (0, 1)
```

```
$ ./task02 abc
stderr: Error: epsilon must be a real number in (0, 1)
```

```
$ ./task02 
stderr: Usage: /tmp/t02 <epsilon>
```

