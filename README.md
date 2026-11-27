# eCookBook

Kuchenny zeszyt z przepisami na ekranie e-ink (Seeed reTerminal E1001). Przepisy są
przechowywane lokalnie na karcie SD, pobiera się je z serwera domowego, a przegląda po tagach.

Docelowy efekt opisuje [docs/vision.md](docs/vision.md).

## Firmware

Wymagany jest [PlatformIO](https://platformio.org/). E1001 podłączasz kablem USB-C z
transmisją danych i budzisz zielonym przyciskiem.

```bash
cd firmware
pio run -t upload
pio device monitor
```

Testy logiki uruchamiane na komputerze: `pio test -e native`.

Obecny stan: etap 3. Urządzenie czyta wszystkie przepisy z folderu `/recipes` na karcie
microSD (FAT32). Przeglądasz je ścieżką tagi → lista przepisów → przepis; skrót obsługi
jest w [docs/vision.md](docs/vision.md). Przykładowe przepisy są w [recipes/](recipes/):
skopiuj je do `/recipes` na karcie.
