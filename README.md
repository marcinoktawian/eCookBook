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

Obecny stan: etap 2. Urządzenie wyświetla pierwszy przepis z folderu `/recipes` na karcie
microSD (FAT32) i dzieli go na strony. Lewy i prawy przycisk przewracają strony.
Przykładowy przepis jest w [recipes/zurek.md](recipes/zurek.md).
