# Serwer eCookBook

`serve.py` przechowuje przepisy jako pliki `.md` w jednym folderze, udostępnia API JSON
i aplikację WWW z `web/index.html`. Używa tylko biblioteki standardowej Pythona.

## Lokalnie

```bash
python3 server/serve.py --dir recipes --port 8082
python3 -m unittest discover server
```

## Na pi-nas (OMV)

- **Stos:** `ecookbook` we wtyczce Compose w OMV, z treścią z [compose.yml](compose.yml).
- **Kod:** `appdata/ecookbook/app/`, czyli kopia `serve.py` i `web/`, montowana tylko do odczytu.
- **Przepisy:** `appdata/ecookbook/recipes/`. Usunięte przepisy trafiają do `recipes/.trash/`.
- **Adres:** `http://ecookbook.pi-nas.lan` przez Caddy (blok w `appdata/caddy/Caddyfile`).
  Strona jest dostępna dla wszystkich domowników w sieci domowej i przez Tailscale.

Aktualizacja kodu:

```bash
scp -r server/serve.py server/web pi-nas@192.168.0.199:/tmp/ecookbook-app/
ssh pi-nas@192.168.0.199 'sudo cp -r /tmp/ecookbook-app/. /srv/dev-disk-by-uuid-90cee710-436c-4114-89da-3bba871b1961/appdata/ecookbook/app/ && docker restart ecookbook'
```
