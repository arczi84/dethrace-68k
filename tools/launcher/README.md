# DethraceLauncher

Natywny launcher Intuition/GadTools dla amigowej wersji Dethrace. Nie wymaga
MUI ani SDL.

Launcher zapamiętuje ustawienia w `dethrace-launcher.cfg` obok programu i
uruchamia binarkę `dethrace` z parametrem `--use-cfg`. Obsługuje układ release,
w którym dane podstawowej gry są w `CARMA`, Splat Pack w `CARSPLAT`, demo
Carmageddon w `CARMDEMO`, a demo Splat Pack w `SPLATDEMO`.

Launcher pozwala wybrać renderer, tryb ekranu, limit i wyświetlanie FPS,
dźwięk, szczegółowość oraz najważniejsze opcje rozgrywki. MiniGL działa w
640x480, a HAM6 w 320x200, dlatego wybór rozdzielczości jest w tych trybach
automatycznie blokowany.

Kompilacja:

```sh
make
```

Skopiuj `DethraceLauncher` do głównego katalogu release, obok binarki
`dethrace` i katalogów z danymi gier.
