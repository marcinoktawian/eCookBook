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
| Prawo | następna pozycja lub strona | menu (synchronizacja, informacje) |
| Wybierz (zielony) | otwórz; w przepisie: przeskok składniki ⇄ przygotowanie | uśpienie |

- **Tagi:** lista tagów, np. `mięso`, `wegetariańskie`, `włoskie`, `azjatyckie`, z liczbą
  przepisów przy każdym. Jest też pozycja „Wszystkie”.
- **Lista przepisów:** tytuły przepisów z wybranym tagiem, ułożone alfabetycznie.
- **Przepis:** jedna kolumna, podzielona na strony. Otwiera się na składnikach, a po nich
  idzie przygotowanie. Zielony przycisk z dowolnej strony przygotowania przeskakuje do
  składników, a drugie naciśnięcie wraca dokładnie na tę stronę, z której przyszedłeś.
  Ze składników, gdy nic nie jest zapamiętane, przeskakuje na początek przygotowania.
  W nagłówku jest tytuł, nazwa sekcji i numer strony.
- **Usypianie:** gdy żaden przycisk nie jest używany przez kilka minut, urządzenie przechodzi
  w głęboki sen. Ekran e-ink dalej pokazuje ostatnią stronę, więc przepis widać podczas
  gotowania. Budzi je dowolny przycisk.

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

## Synchronizacja

```
serwer domowy (zwykły HTTP, pliki statyczne)      E1001
  recipes/*.md                                   ──▶  /sd/recipes/*.md
  index.json  (tytuł, tagi, skrót pliku)         ──▶  /sd/index.json
```

- **Serwer:** zwykły serwer plików statycznych (nginx albo `python -m http.server`) plus
  skrypt, który buduje `index.json`. Nie trzeba pisać własnego backendu.
- **Urządzenie:** pobiera `index.json` i ściąga tylko te pliki, których skrót się zmienił.
  Synchronizację uruchamia się ręcznie z menu.
- **Praca bez sieci:** urządzenie zawsze czyta przepisy z karty SD.

## Czego świadomie nie robimy

- **Wyszukiwania po tekście.** Na 3 przyciskach nie da się wygodnie pisać, więc zamiast tego
  przeglądamy przepisy po tagach.
- **Zdjęć potraw.** Ekran jest czarno-biały i nie ma odcieni szarości.
- **Edycji przepisów na urządzeniu.** Przepisy edytuje się na komputerze.
- **Dotyku.** E1001 go nie ma.

## Etapy

0. **Hello world:** tekst na ekranie.
1. **Przyciski:** krótkie i długie naciśnięcia.
2. **Jeden przepis z karty SD:** z polskimi znakami (czcionka UTF-8) i podziałem na strony.
3. **Nawigacja:** tagi, lista przepisów, przepis.
4. **Synchronizacja z serwerem domowym.**
5. **Usypianie i pomiar baterii.**
6. **Dopracowanie wyglądu:** wybór lepszej czcionki. Na razie jest New Century Schoolbook
   z U8g2 (`ncen*_te`), która działa, ale warto porównać inne.
