# eCookBook

Kuchenny zeszyt z przepisami na ekranie e-ink (Seeed reTerminal E1001). Przepisy są
przechowywane lokalnie na karcie SD, pobiera się je z serwera domowego, a przegląda po tagach.

Docelowy efekt opisuje [docs/vision.md](docs/vision.md).

## Serwer i aplikacja WWW

Przepisy przeglądasz, dodajesz i edytujesz w przeglądarce pod `http://ecookbook.pi-nas.lan`
(sieć domowa i Tailscale). Kod, testy i opis wdrożenia na pi-nas są w [server/](server/README.md).

## Firmware

Wymagany jest [PlatformIO](https://platformio.org/). E1001 podłączasz kablem USB-C z
transmisją danych i budzisz zielonym przyciskiem.

```bash
cd firmware
pio run -t upload
pio device monitor
```

Testy logiki uruchamiane na komputerze: `pio test -e native`.

Obecny stan: etap 4. Urządzenie czyta przepisy z folderu `/recipes` na karcie microSD
(FAT32), a przeglądasz je ścieżką tagi → lista przepisów → przepis (skrót obsługi w
[docs/vision.md](docs/vision.md)).

### Synchronizacja z serwerem

1. Skopiuj [firmware/config.example.txt](firmware/config.example.txt) na kartę jako
   `/config.txt` i wpisz nazwę sieci Wi-Fi (2,4 GHz) i hasło, bez cudzysłowów.
2. Przytrzymaj prawy przycisk. Urządzenie pobierze nowe i zmienione przepisy, usunie te,
   których nie ma już na serwerze, i wyłączy Wi-Fi.

Szczegóły połączenia (przyczyna błędu Wi-Fi, widoczne sieci) widać w `pio device monitor`.
