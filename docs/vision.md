# eCookBook: docelowy efekt

Kuchenny zeszyt z przepisami na ekranie e-ink. Stoi na blacie, działa tygodniami na baterii,
przepisy trzyma lokalnie, a nowe pobiera z serwera domowego.

## Sprzęt

Seeed reTerminal E1001:

- ESP32-S3 (8 MB PSRAM, 32 MB flash)
- ekran 7,5" 800×480, czarno-biały
- 3 przyciski, microSD, Wi-Fi, bateria 2000 mAh

## Jak się z tego korzysta

```
[Tagi]  ──wybierz──▶  [Lista przepisów]  ──wybierz──▶  [Przepis]
  ▲                         │    ▲                          │
  └──── długie lewo ────────┘    └────── długie lewo ───────┘
```

| Przycisk | Krótko | Długo |
|---|---|---|
| Lewo | poprzednia pozycja lub strona | cofnij |
| Prawo | następna pozycja lub strona | synchronizacja z serwerem |
| Wybierz (zielony) | otwórz; w przepisie: przeskok składniki ⇄ przygotowanie | uśpienie; ponad 3 s: tapeta i uśpienie |

- **Tagi:** lista tagów, np. `mięso`, `wegetariańskie`, `włoskie`, `azjatyckie`, z liczbą
  przepisów przy każdym. Jest też pozycja „Wszystkie”.
- **Lista przepisów:** tytuły przepisów z wybranym tagiem, ułożone alfabetycznie.
- **Przepis:** jedna kolumna, podzielona na strony. Otwiera się na składnikach, a po nich
  idzie przygotowanie. Zielony przycisk z dowolnej strony przygotowania przeskakuje do
  składników, a drugie naciśnięcie wraca dokładnie na tę stronę, z której przyszedłeś.
  Ze składników, gdy nic nie jest zapamiętane, przeskakuje na początek przygotowania.
  W nagłówku jest tytuł, nazwa sekcji i numer strony.
- **Usypianie:** po 3 minutach bez naciśnięcia albo po przytrzymaniu zielonego urządzenie
  przechodzi w głęboki sen. Ekran odświeża się z księżycem w prawym dolnym rogu i dalej
  pokazuje ostatnią stronę, więc przepis widać podczas gotowania. Dowolny przycisk budzi
  urządzenie na tej samej stronie.
- **Tapeta:** zielony przytrzymany ponad 3 sekundy pokazuje losowe zdjęcie JPG z folderu
  `/wallpaper` na karcie (przycięte do ekranu, czarno-białe z rozpraszaniem) i usypia
  urządzenie. Po obudzeniu zaczyna się od listy tagów.
- **Bateria:** procent naładowania w nagłówku listy tagów.

## Przepisy

Jeden przepis to jeden plik Markdown z nagłówkiem YAML:

```markdown
---
title: Spaghetti carbonara
tags: [włoskie, mięso]
---

## Składniki
- 200 g spaghetti
- 100 g guanciale

## Przygotowanie
1. Ugotuj makaron al dente.
```

Urządzenie obsługuje tylko część Markdowna: nagłówki, listy i zwykłe akapity.

## Serwer i aplikacja WWW

```
             pi-nas: kontener ecookbook (Python, bez zależności)
             ┌─────────────────────────────────────────────┐
 telefon ───▶│  aplikacja WWW (jeden plik HTML + JS)       │
 laptop  ───▶│  /api/recipes   czytaj, dodaj, edytuj, usuń │──▶ folder recipes/*.md
 E1001  ────▶│  /index.json    synchronizacja urządzenia   │
             └─────────────────────────────────────────────┘
```

- **Jedno źródło prawdy:** pliki `.md` w folderze na NAS-ie. Nie ma bazy danych.
- **Aplikacja WWW:** `http://ecookbook.pi-nas.lan`, dostępna w sieci domowej i przez
  Tailscale. Nie jest wystawiona do internetu, więc nie ma osobnego logowania.
- **Edycja przez formularz:** tytuł, tagi, składniki (jeden w linii) i kroki (jeden w linii).
  Serwer zamienia to na format `.md` opisany wyżej, żeby nikt nie musiał znać Markdowna.
- **Bezpieczne zapisy:** plik jest najpierw zapisywany obok, a potem podmieniany, więc
  urządzenie nie pobierze połowy pliku. Usunięcie przenosi przepis do `recipes/.trash/`.
- **Lista zakupów:** w przepisie wybierasz „Dodaj do listy zakupów”, a lista zbiera składniki
  pogrupowane według przepisów. Odznaczasz to, co już masz, i kopiujesz albo pobierasz
  listę jako `.txt`. „Udostępnij” (systemowe menu telefonu) pojawi się dopiero po
  włączeniu HTTPS, bo przeglądarki udostępniają je tylko na bezpiecznych stronach.
  Lista jest zapisywana osobno na każdym urządzeniu.

## Synchronizacja urządzenia

- **Urządzenie:** pobiera `index.json` i ściąga tylko te pliki, których skrót się zmienił.
  Przepisy usunięte z serwera znikają z karty. Synchronizację uruchamia się ręcznie.
- **Praca bez sieci:** urządzenie zawsze czyta przepisy z karty SD.

## Czego świadomie nie robimy

- **Wyszukiwania po tekście.** Na 3 przyciskach nie da się wygodnie pisać, więc zamiast tego
  przeglądamy przepisy po tagach.
- **Zdjęć potraw.** Ekran jest czarno-biały i nie ma odcieni szarości.
- **Edycji przepisów na urządzeniu.** Przepisy edytuje się w aplikacji WWW.
- **Sumowania składników na liście zakupów.** Ilości są pisane ręcznie („2 łyżki”, „szczypta”),
  więc rozpoznawanie jednostek dawałoby błędy.
- **Dotyku.** E1001 go nie ma.

## Etapy

0. **Hello world:** tekst na ekranie.
1. **Przyciski:** krótkie i długie naciśnięcia.
2. **Jeden przepis z karty SD:** z polskimi znakami (czcionka UTF-8) i podziałem na strony.
3. **Nawigacja:** tagi, lista przepisów, przepis.
4. **Serwer i aplikacja WWW na pi-nas:** przeglądanie, dodawanie, edycja i usuwanie
   przepisów, a potem lista zakupów i synchronizacja urządzenia z serwerem.
5. **Usypianie i pomiar baterii.**
6. **Dopracowanie wyglądu:** wybór lepszej czcionki. Na razie jest New Century Schoolbook
   z U8g2 (`ncen*_te`), która działa, ale warto porównać inne.
