# Neutrino Mediathek API

Dies ist die Web-API, die vom Neutrino-Mediathek-Plugin abgefragt wird. Sie
stellt JSON- und HTML-Ausgaben bereit, die auf einer MariaDB-Datenbank mit den
aufbereiteten MediathekView-Daten basieren.

## Funktionen auf einen Blick

- REST-ähnliche Endpunkte (`mode=api&sub=…`) für Filmlisten, Sender und
  Livestreams.
- Schlanke CGI-Integration über `mt-api.cgi` oder FastCGI (spawn-fcgi,
  lighttpd/nginx).
- Rein statische Assets (HTML, CSS, JS), die auf jedem Webserver ausgeliefert
  werden können.
- Keine externen Abhängigkeiten mehr zu LLVM oder Boost URI – das Binary linkt
  ausschließlich gegen libc/pthread/dl.

## Schnellstart (Docker)

```bash
# Abhängige Projekte klonen (Importer + API)
make vendor

# MariaDB starten
docker-compose up -d db

# Importer einmal laufen lassen, damit Daten vorhanden sind
docker-compose run --rm importer --update
docker-compose run --rm importer

# API auf Port 8080 im Container / 18080 auf dem Host starten
docker-compose up -d api

# Testaufruf
curl http://localhost:18080/mt-api?mode=api&sub=info
```

Die dazugehörigen Compose-Dateien findest du im übergeordneten
`services/mediathek-backend`-Verzeichnis aus dem `neutrino-make` Projekt. Für
alleinstehende Builds siehe Abschnitt "Manuell bauen".

## Vorgefertigtes Docker-Image

Über GitHub Actions wird ein Multi-Arch-Image nach Docker Hub veröffentlicht:
`docker pull dbt1/mt-api-dev:latest`. Es enthält Binary, statische Assets
und eine vorkonfigurierte lighttpd-Instanz. Verbinden kannst du es mit einer
bestehenden MariaDB über:

```bash
docker run -d --name mt-api \
  -e MT_API_DB_HOST=db.example.org \
  -p 18080:8080 \
  dbt1/mt-api-dev:latest
```

Ein Update läuft ebenfalls über `docker pull` + Neustart:

```bash
docker pull dbt1/mt-api-dev:latest
docker stop mt-api && docker rm mt-api
docker run -d --name mt-api \
  -e MT_API_DB_HOST=db.example.org \
  -p 18080:8080 \
  dbt1/mt-api-dev:latest
```

Optional lassen sich `/opt/api/data` und `/opt/api/log` per Volume persistieren.
Details zum Build stehen in `.github/workflows/docker-publish.yml`.

Beispiel kompletter Ablauf (Raspberry Pi oder PC) zusammen mit dem Importer:

```bash
docker run --rm \
  -v $PWD/config/importer:/opt/importer/config \
  -v $PWD/data/importer:/opt/importer/bin/dl \
  --network mediathek-net \
  dbt1/mediathek-importer --update

docker run --rm \
  -v $PWD/config/importer:/opt/importer/config \
  -v $PWD/data/importer:/opt/importer/bin/dl \
  --network mediathek-net \
  dbt1/mediathek-importer

> Hinweis: `mediathek-net` steht exemplarisch für ein vorhandenes Docker-Netz
> (z. B. aus `docker compose`). Wenn du direkt auf den lokalen Host zugreifen
> willst, kannst du stattdessen `--network host` verwenden. Für dauerhafte
> Container einfach `--name <NAME>` setzen und `--rm` weglassen.

docker run -d --name mt-api \
  --network mediathek-net \
  -p 18080:8080 \
  -e MT_API_DB_HOST=db \
  dbt1/mt-api-dev:latest
```

Die Images sind als Multi-Arch-Builds verfügbar (amd64 & arm64) – die gleichen
Kommandos funktionieren daher auf Desktop-PCs und Raspberry Pi.

## Voraussetzungen

- GCC 10+ oder Clang 11+
- MariaDB Connector/C (`libmariadb-dev`)
- libcurl, libtidy, libjsoncpp, libboost-system, libfcgi
- `sassc` (für das Generieren der CSS-Dateien)

Unter Debian/Ubuntu genügt:

```bash
sudo apt install build-essential pkg-config git libmariadb-dev \
  libcurl4-openssl-dev libfcgi-dev libssl-dev rapidjson-dev \
  libtidy-dev libjsoncpp-dev libboost-system-dev sassc
```

## Manuelles Bauen

```bash
git clone https://github.com/tuxbox-neutrino/mt-api-dev.git
cd mt-api-dev
cp doc/config.mk.sample config.mk
sed -i 's/USE_COMPILER\t\t= CLANG/USE_COMPILER\t\t= GCC/' config.mk
make -j$(nproc)
```

Das fertige Binary liegt anschließend unter `build/mt-api`. Die statischen
Assets werden nach `build/src/css` sowie `src/web/...` kopiert. Für eine einfache
Installation kannst du `make install DESTDIR=/opt/api.dist` verwenden.

> **Version hinterlegen**  
> Standardmäßig wird `git describe --tags` in das Binary geschrieben. Beim
> Bauen aus einem Tarball kannst du das überschreiben:  
> `MT_API_VERSION=0.5.0 make`. Die Ausgabe erscheint als `apiversion` im
> Endpunkt `/mt-api?mode=api&sub=info`.

## Konfiguration & Betrieb

1. Kopiere `sqlpasswd` nach `/opt/api/data/.passwd/sqlpasswd` und trage den
   MariaDB-Benutzer ein, der Leserechte auf die Mediathek-Datenbank hat.
2. `mt-api.cgi` erwartet die Umgebungsvariable `DOCUMENT_ROOT` (vom Webserver
   gesetzt) und greift auf `/opt/api/bin/mt-api` zu.
3. Für FastCGI empfiehlt sich:
   ```bash
   spawn-fcgi -s /run/mt-api.sock -u www-data -g www-data /opt/api/bin/mt-api
   ```
4. Das beiliegende `docker/api/lighttpd.conf` demonstriert eine funktionierende
   lighttpd-Konfiguration.

## Entwicklung & Tests

- `make clean && make` – Neubau
- `make css` – nur die SCSS-Dateien neu generieren
- `make lint` – (WIP) geplanter Stil-Check
- Für End-to-End-Tests empfiehlt sich das `make smoke` Target im
  `services/mediathek-backend`-Root.

## Versionierung

Wir folgen SemVer. Dieser Stand wurde als **v0.2.0** getaggt und enthält die
Umstellung auf reine Standardbibliotheken sowie besser dokumentierte
Quickstart-Anleitungen. Ältere Versionen findest du über `git tag`.

## Support

Fragen oder Beiträge bitte als Issue oder Pull Request auf GitHub einreichen:
<https://github.com/tuxbox-neutrino/mt-api-dev>.
