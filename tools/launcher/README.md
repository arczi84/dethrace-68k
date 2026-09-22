# DethraceLauncher

Natywny launcher Intuition/GadTools dla amigowej wersji Dethrace. Nie wymaga
MUI ani SDL.

Launcher zapamiętuje ustawienia w `dethrace-launcher.cfg` obok programu i
uruchamia binarkę `dethrace` z parametrem `--use-cfg`. Obsługuje układ release,
w którym dane podstawowej gry są w `CARMA`, Splat Pack w `CARSPLAT`, demo
Carmageddon w `CARMDEMO`, a demo Splat Pack w `SPLATDEMO`.

Launcher pozwala wybrać renderer, rozdzielczość, tryb ekranu, limit i wyświetlanie
FPS, dźwięk, szczegółowość oraz najważniejsze opcje rozgrywki. MiniGL obsługuje
wyjścia od 640x480 do 1920x1080; układ interfejsu gry pozostaje 640x480.
HAM6 wymusza 320x200.

Pole **Zasieg widzenia / Draw distance** wybiera mnożnik x1, x1.5, x2 lub x3.
Domyślne x1 zachowuje dotychczasowy zasięg. Przy `Yon 35` mnożniki dają
odpowiednio 35, 52.5, 70 i 105, przed zastosowaniem ustawienia danej trasy.
Większy zasięg może obniżyć FPS.

Ustawienie jest zapisywane jako `draw_distance=0..3` w `dethrace-launcher.cfg`.
Brak wpisu oznacza x1. Gra odczytuje je przez `--use-cfg`; wymagany jest nowy
launcher **i** gra `0.10.1-amiga-r10-hedeon2` lub nowsza. Starsze EXE, w tym
wcześniejsze warianty porównawcze GCC16, nie obsługują tego pola. Mnożnik działa
na kamery i odległości mgły, pozostawiając bazowy `Yon` i zakres menu gry bez
zmian. Nie zmienia kamer galerii ani interfejsu 2D.

Kompilacja:

```sh
make
```

Skopiuj `DethraceLauncher` do głównego katalogu release, obok binarki
`dethrace` i katalogów z danymi gier.
