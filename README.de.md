# Neutrino Mediathek API

Dies ist die Web-API, die vom Neutrino-Mediathek-Plugin abgefragt wird. Sie
stellt JSON- und HTML-Ausgaben bereit, die auf einer MariaDB-Datenbank mit den
aufbereiteten MediathekView-Daten basieren.

## Architektur-Überblick

Das Gesamtsystem besteht aus drei Bausteinen:

1. **MariaDB** – speichert die MediathekView-Daten (`mediathek_1` Tabellen).
   Entweder nutzt du einen vorhandenen Server oder lässt das Schnellstart-Skript
   einen `mariadb:11.4` Container starten.
2. **Importer (`dbt1/mediathek-importer`)** – lädt die Filmlisten herunter und
   schreibt sie in MariaDB. Einmalig `--update` ausführen, danach zyklisch per
   `--cron-mode`, damit die Tabellen aktuell bleiben.
3. **API (`dbt1/mt-api-dev`)** – arbeitet rein lesend und stellt die Daten über
   CGI/FastCGI dem Neutrino-Plugin bereit. Sie benötigt eine bereits gefüllte
   MariaDB-Instanz, erreichbar über `MT_API_DB_HOST` plus Benutzer/Passwort.

Da der API-Container keine Datenbank enthält, lautet die empfohlene Reihenfolge
**(1) MariaDB bereitstellen → (2) Importer laufen lassen → (3) API starten**.
`scripts/quickstart.sh` automatisiert diese Abfolge auf einem Host.

## Inhaltsverzeichnis

- [Schnellstart-Skript](#schnellstart-skript)
- [Funktionen auf einen Blick](#funktionen-auf-einen-blick)
- [Schnellstart (Docker)](#schnellstart-docker)
- [Vorgefertigtes Docker-Image](#vorgefertigtes-docker-image)
- [Voraussetzungen](#voraussetzungen)
- [Manuelles Bauen](#manuelles-bauen)
- [Konfiguration & Betrieb](#konfiguration--betrieb)
- [Entwicklung & Tests](#entwicklung--tests)
- [Versionierung](#versionierung)
- [Support](#support)

## Schnellstart-Skript

Nutze dieses Skript, wenn Importer und API möglichst automatisiert mit Docker
aufgesetzt werden sollen.

Ein Einzeiler richtet Importer + API automatisch komplett ein:

```bash
curl -fsSL https://raw.githubusercontent.com/tuxbox-neutrino/mt-api-dev/master/scripts/quickstart.sh -o quickstart.sh
chmod +x quickstart.sh
./quickstart.sh
```

Während der Ausführung fragt das Skript nach MariaDB-Host/-Benutzer/-Passwort
(Standard: `root` / `example-root`). Ändere das Passwort einfach während der 
Abfrage. Standardmäßig startet das Script automatisch einen
lokalen MariaDB-Container (`mariadb:11.4`), legt die benötigten Dateien unter
`config/importer/` bzw. `config/api/` an, zieht die Docker-Images und startet
anschließend Importer **und** API mit `--restart unless-stopped`.

Fertig, mehr braucht man normalerweise nicht mehr zu tun und der API-Server ist
bereit.

Alle Werte lassen sich über Umgebungsvariablen anpassen – z. B. wenn MariaDB
schon auf dem Host läuft:

```bash
NETWORK_MODE=host MT_API_DB_HOST=192.168.1.50 ./quickstart.sh
```

Nach Abschluss des Skripts liegen neue Konfigurationsdateien in
`config/importer/` (Importer), `config/api/` (SQL-Passwort) sowie die Downloads
unter `data/importer/`. Importer und API laufen anschließend dauerhaft im
Hintergrund (`--restart unless-stopped`). Überprüfe den Status zum Beispiel mit:

```bash
docker ps --filter name=mediathek
curl http://localhost:18080/mt-api?mode=api&sub=info
docker logs mediathek-importer
docker logs mediathek-api
```

Wenn du Konfiguration oder Netzwerkparameter ändern möchtest, kannst du das
Skript jederzeit erneut starten – bestehende Verzeichnisse werden wiederverwendet.

Standardwerte des Skripts (über Umgebungsvariablen oder Eingaben anpassbar):

- Docker-Netzwerk: `mediathek-net` (Bridge) bzw. bei `NETWORK_MODE=host` direkt
  das Host-Netzwerk
- API-Port auf dem Host: `18080`
- Container-Namen: `mediathek-db`, `mediathek-importer`, `mediathek-api`
- MariaDB-Zugangsdaten: Benutzer `root`, Passwort `example-root`
- MariaDB-Host: `mediathek-db`, sofern die integrierte Datenbank gestartet wird,
  andernfalls der Wert aus der Host-Abfrage

### Hinweise zu Zugangsdaten

- Die Defaults (`root` / `example-root`) sind **nur für lokale Tests gedacht**.
  Setze deine eigenen Werte über `DB_USER`, `DB_PASS` (und bei Bedarf
  `DB_ROOT_PASS`) oder ändere sie bei der Abfrage, bevor du den Dienst ins Netz
  stellst.
- Startest du die integrierte MariaDB, legt das Skript den gewünschten Benutzer
  automatisch an und vergibt Rechte auf `mediathek_*`. Bei externen Datenbanken
  müssen Benutzer/Passwort bereits existieren – das Skript übernimmt sie nur in
  die Konfigurationsdateien.
- Beispiel:  
  `DB_USER=mediathek DB_PASS=$(openssl rand -hex 12) DB_ROOT_PASS=myrootpw ./quickstart.sh`
  erzeugt in einem Durchlauf eigene Zugangsdaten.
- `config/importer/pw_mariadb` sowie `config/api/sqlpasswd` werden bei jedem
  Lauf überschrieben, damit geänderte Passwörter sofort aktiv sind.

### Datenbank-Wartezeit & Troubleshooting

- Auf Geräten wie dem Raspberry Pi benötigt der erste MariaDB-Start gern bis zu
  90 Sekunden. Der Spinner hinter „Waiting for MariaDB …“ signalisiert, dass das
  Skript weiter Polling betreibt; erst nach erfolgreichem Ping geht es mit dem
  Importer weiter.
- Direkt nach `--update` führt das Skript automatisch einmal `mv2mariadb --force-convert`
  aus und befüllt damit `mediathek_1` mit der aktuellen Filmliste (Download ~2 GB,
  Dauer je nach Bandbreite mehrere Minuten).
- Musste das Skript abbrechen, kannst du die Schritte manuell fortsetzen:
  1. Mit `docker ps` und `docker logs mediathek-db` sicherstellen, dass MariaDB
     läuft.
  2. `docker run --rm … dbt1/mediathek-importer --update` ausführen, um die
     Tabellen zu erzeugen.
  3. Dauerhafte Importer- und API-Container wie unten beschrieben starten.

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

Wenn du ohnehin im `neutrino-make`-Repository arbeitest, bildet dieser
Compose-Stack die spätere Umgebung 1:1 ab.

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

Für eigenständige Installationen (Raspberry Pi, Root-Server, Heimserver) kannst
du direkt das veröffentlichte OCI-Image verwenden.

Über GitHub Actions wird ein Multi-Arch-Image nach Docker Hub veröffentlicht:
`docker pull dbt1/mt-api-dev:latest`. Es enthält Binary, statische Assets
und eine vorkonfigurierte lighttpd-Instanz. Verbinden kannst du es mit einer
bestehenden MariaDB über:

```bash
docker run -d --name mt-api \
  -e MT_API_DB_HOST=db \
  -p 18080:8080 \
  dbt1/mt-api-dev:latest
```

Wähle dabei einen Hostnamen, der zu deiner Umgebung passt:

- `db` – Standardname innerhalb eines Docker-Compose-Netzes.
- `127.0.0.1` / `localhost` – wenn API und MariaDB auf demselben Host mit
  `--network host` laufen.
- `192.168.1.50` – typisches Beispiel für einen externen Server im Heimnetz.

Ein Update läuft ebenfalls über `docker pull` + Neustart:

```bash
docker pull dbt1/mt-api-dev:latest
docker stop mt-api && docker rm mt-api
docker run -d --name mt-api \
  -e MT_API_DB_HOST=192.168.1.50 \
  -p 18080:8080 \
  dbt1/mt-api-dev:latest
```

Optional lassen sich `/opt/api/data` und `/opt/api/log` per Volume persistieren.
Details zum Build stehen in `.github/workflows/docker-publish.yml`.

Beispiel kompletter Ablauf (Raspberry Pi oder PC) zusammen mit dem Importer:

```bash
# Komplettstart über Skript
./scripts/quickstart.sh

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

Die Umgebungsvariable `MT_API_DB_HOST` muss auf den MariaDB-Server zeigen, der
die importierten Tabellen bereitstellt (Hostname oder IP). Bei `--network host`
entsprechend `MT_API_DB_HOST=localhost` oder die tatsächliche IP setzen.

Weitere Details zum Importer siehe README im Schwesterprojekt
`tuxbox-neutrino/db-import` (lokal: `services/mediathek-backend/vendor/db-import/README.md`).

## Voraussetzungen

Für einen manuellen Build benötigst du folgende Werkzeuge und Bibliotheken.

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

So kompilierst du das Projekt lokal mit GCC oder Clang:

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

Nach der Installation bindest du das Binary wie folgt in deinen Webserver ein:

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

Diese Targets erleichtern die tägliche Entwicklung:

- `make clean && make` – Neubau
- `make css` – nur die SCSS-Dateien neu generieren
- `make lint` – (WIP) geplanter Stil-Check
- Für End-to-End-Tests empfiehlt sich das `make smoke` Target im
  `services/mediathek-backend`-Root.

## Versionierung

Wir veröffentlichen getaggte Releases, damit das Plugin die Backend-Version
prüfen kann.

Wir folgen SemVer. Dieser Stand wurde als **v0.2.0** getaggt und enthält die
Umstellung auf reine Standardbibliotheken sowie besser dokumentierte
Quickstart-Anleitungen. Ältere Versionen findest du über `git tag`.

## Support

Fragen oder Beiträge bitte als Issue oder Pull Request auf GitHub einreichen:
<https://github.com/tuxbox-neutrino/mt-api-dev>.
