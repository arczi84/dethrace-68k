# Dethrace Amiga 68k 0.10.1 — pierwsze wydanie MiniGL

To wydanie zastępuje starszy port amigowy oparty na SDL 1.2 i Dethrace 0.8.
Gra została przeniesiona na Dethrace 0.10.1 i korzysta teraz bezpośrednio z
AmigaOS.

## Najważniejsze nowości na Amidze

- Pierwszy sprzętowy renderer MiniGL dla Dethrace na Amidze.
- Natywna obsługa ekranu, klawiatury i myszy — SDL nie jest już wymagane.
- Obsługa CyberGraphX 8-bit, natywnego AGA oraz HAM6.
- Wybór trybu ekranu przez systemowy requester ASL.
- Efekty dźwiękowe i muzyka odtwarzane przez AHI.
- Gra działa z katalogu `PROGDIR:` i nie wymaga zamontowanej fizycznej płyty CD.
- Poprawiona obsługa nazw plików różniących się wielkością liter.
- Dołączony natywny launcher z obsługą Carmageddon, Splat Pack oraz ich wersji
  demonstracyjnych.

## MiniGL i renderer software

- MiniGL włącza się opcją `--opengl`.
- Klasyczny renderer software pozostaje w tej samej binarce i działa bez opcji
  `--opengl`.
- Na testowanym zestawie MiniGL osiągał do około 55 FPS.
- Renderer software również został nieznacznie przyspieszony.
- Dostępny jest wariant korzystający ze współdzielonej `minigl.library`.

## Zmiany Dethrace 0.8 → 0.10.1

- Znacznie rozszerzony tryb Action Replay, obejmujący więcej zdarzeń z wyścigu.
- Pełniejsza obsługa power-upów, ich komunikatów i elementów HUD-u.
- Rozbudowane zakończenie wyścigu, ekran podsumowania i Wrecks Gallery.
- Ulepszone zachowanie przeciwników oraz obsługa uszkodzeń samochodu.
- Lepsza zgodność z wersjami demonstracyjnymi i dodatkiem Splat Pack.
- Liczne poprawki stabilności menu, kamer, dźwięku, wyścigów i powtórek.

Rzeczywista wydajność zależy od procesora, karty graficznej, rozdzielczości i
wyświetlanej sceny.
