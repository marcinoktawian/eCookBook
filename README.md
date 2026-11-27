# eCookBook

Kuchenny zeszyt z przepisami na ekranie e-ink. Czytnik [Seeed reTerminal E1001](https://wiki.seeedstudio.com/getting_started_with_reterminal_e1001/)
stoi na blacie, działa na baterii i trzyma przepisy na karcie microSD. Przepisy dodaje się
i edytuje w aplikacji WWW na serwerze domowym, a czytnik pobiera je jednym przyciskiem.

```
 telefon / laptop ──▶ aplikacja WWW ──▶ pi-nas: folder recipes/*.md ──▶ E1001 (karta SD)
                      (przeglądanie,       (jedyne źródło prawdy)        (przytrzymaj prawy
                       edycja, lista                                      = synchronizacja)
                       zakupów)
```

## Co potrafi

**Czytnik E1001**
- Przeglądanie przepisów po tagach: tagi → lista przepisów → przepis, w polskiej kolejności
  alfabetycznej, z polskimi znakami.
- Przepis podzielony na strony. Zielony przycisk przeskakuje do składników i wraca na stronę,
  z której przyszedłeś.
- Synchronizacja z serwerem przez Wi-Fi: pobiera tylko nowe i zmienione przepisy, każdy
  sprawdzany skrótem SHA-256.
- Głęboki sen po 3 minutach albo na żądanie. Ekran dalej pokazuje stronę, z księżycem
  w rogu, a urządzenie budzi się na tej samej stronie.
- Tapeta: losowe zdjęcie z karty, pokazywane przed uśpieniem.
- Poziom baterii w nagłówku listy tagów.

**Aplikacja WWW** (`http://ecookbook.pi-nas.lan`, sieć domowa i Tailscale)
- Lista przepisów z filtrem tagów i wyszukiwarką w tytułach, tagach i składnikach.
- Dodawanie i edycja przez formularz, bez znajomości Markdowna. Usunięte przepisy trafiają
  do kosza na serwerze.
- Lista zakupów: zbiera składniki wybranych przepisów, a to, co już masz, odznaczasz.
  Listę można skopiować albo pobrać jako `.txt`.

## Obsługa czytnika

| Przycisk | Krótko | Przytrzymanie |
|---|---|---|
| Lewy | poprzednia pozycja / strona | wstecz |
| Prawy | następna pozycja / strona | synchronizacja z serwerem |
| Zielony | otwórz; w przepisie: składniki ⇄ przygotowanie | uśpienie; ponad 3 s: tapeta i uśpienie |

Listy zawijają się (z ostatniej pozycji na pierwszą), a strony przepisu zatrzymują się na
końcach. Każdy przycisk budzi uśpione urządzenie.

## Karta microSD (FAT32)

```
/config.txt        Wi-Fi i adres serwera (wzór: firmware/config.example.txt)
/recipes/*.md      przepisy; wypełnia je synchronizacja
/wallpaper/*.jpg   zdjęcia na ekran uśpienia (opcjonalnie, prosto z telefonu)
```

`config.txt` to linie `klucz=wartość`, **bez cudzysłowów**. Znaki specjalne w haśle wpisuje
się zwyczajnie. Czytnik łączy się tylko z Wi-Fi 2,4 GHz.

## Format przepisu

Każdy przepis to plik Markdown z nagłówkiem. Aplikacja WWW zapisuje go w tym formacie sama,
a ręcznie można go edytować np. przez SMB.

```markdown
---
title: Żurek z białą kiełbasą
tags: [zupy, mięso, polskie]
---

## Składniki

- 500 ml zakwasu na żurek
- 300 g białej kiełbasy

## Przygotowanie

1. Podsmaż boczek z cebulą.

2. Wlej bulion i gotuj 20 minut.
```

Przykłady są w [recipes/](recipes/).

## Struktura repozytorium

```
firmware/              czytnik E1001 (PlatformIO, Arduino)
  src/main.cpp           ekrany, przyciski, karta SD, Wi-Fi, sen
  lib/recipe/            logika bez sprzętu: parser, strony, katalog, synchronizacja,
                         bateria, tapeta (testowana na komputerze)
  include/button.h       krótkie i długie naciśnięcia
  test/                  testy Unity uruchamiane na komputerze
server/                serwer i aplikacja WWW (Python, sama biblioteka standardowa)
  serve.py               API przepisów, indeks do synchronizacji, strona
  web/index.html         aplikacja WWW (bez frameworka)
  compose.yml            wdrożenie na pi-nas (OMV Compose za Caddy)
docs/vision.md         opis docelowego efektu i decyzji projektowych
recipes/               przykładowe przepisy
```

## Budowanie i testy

**Firmware** (wymaga [PlatformIO](https://platformio.org/)). E1001 podłączasz kablem USB-C
z transmisją danych i budzisz dowolnym przyciskiem.

```bash
cd firmware
pio run -t upload        # zbuduj i wgraj
pio device monitor       # log: bateria, Wi-Fi, synchronizacja, sen
pio test -e native       # testy logiki na komputerze
```

**Serwer** (wdrożenie na pi-nas opisuje [server/README.md](server/README.md)):

```bash
python3 server/serve.py --dir recipes --port 8082   # lokalnie: http://localhost:8082
python3 -m unittest discover server                 # testy
```

## Stan

Etapy 0–5 z [docs/vision.md](docs/vision.md) są zrobione. Zostało dopracowanie wyglądu
(etap 6), czyli wybór lepszej czcionki na czytniku.

Wiadome ograniczenia:
- Na tapecie obrót zapisany w EXIF jest ignorowany, a JPG progresywne nie są obsługiwane.
- „Udostępnij” na liście zakupów pojawi się dopiero po włączeniu HTTPS. Do tego czasu
  działają „Kopiuj” i „Pobierz .txt”.
- Czas pracy na baterii nie jest jeszcze zmierzony.
