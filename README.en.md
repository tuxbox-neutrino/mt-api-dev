# Neutrino Mediathek API

This repository hosts the backend API consumed by the Neutrino Mediathek plugin.
It exposes lightweight CGI/FastCGI endpoints that render JSON and HTML data
based on the MediathekView catalogue stored in MariaDB.

## Highlights

- REST-ish endpoints (`mode=api&sub=…`) for programme lists, channels and
  livestreams.
- Works with any CGI/FastCGI capable web server (lighttpd, nginx, Apache).
- Ships static HTML/CSS assets alongside the binary.
- No external LLVM / Boost URI dependency anymore – the binary links only
  against libc/pthread/dl.

## Quickstart (Docker)

```bash
# Clone importer/api sources once
make vendor

# Start MariaDB
docker-compose up -d db

# Run importer so the database is populated
docker-compose run --rm importer --update
docker-compose run --rm importer

# Launch the API (container 8080 -> host 18080)
docker-compose up -d api

# Sanity check
curl http://localhost:18080/mt-api?mode=api&sub=info
```

The compose setup lives inside `services/mediathek-backend` of the
`neutrino-make` repository. For standalone builds see "Manual build" below.

## Prebuilt Docker image

GitHub Actions builds and pushes a multi-arch image to Docker Hub:
`docker pull dbt1/mt-api:latest`. It contains the compiled binary,
static assets and a lighttpd setup. Point it to an existing MariaDB instance:

```bash
docker run -d --name mt-api \
  -e MT_API_DB_HOST=db.example.org \
  -p 18080:8080 \
  dbt1/mt-api:latest
```

Persist `/opt/api/data` and `/opt/api/log` via volumes if you want to keep
templates/logs across restarts. See `.github/workflows/docker-publish.yml`
for the automated build definition.

## Requirements

- GCC ≥ 10 or Clang ≥ 11
- MariaDB Connector/C (`libmariadb-dev`)
- libcurl, libtidy, libjsoncpp, libboost-system, libfcgi
- `sassc` for generating CSS

Example for Debian/Ubuntu:

```bash
sudo apt install build-essential pkg-config git libmariadb-dev \
  libcurl4-openssl-dev libfcgi-dev libssl-dev rapidjson-dev \
  libtidy-dev libjsoncpp-dev libboost-system-dev sassc
```

## Manual build

```bash
git clone https://github.com/tuxbox-neutrino/mt-api-dev.git
cd mt-api-dev
cp doc/config.mk.sample config.mk
sed -i 's/USE_COMPILER\t\t= CLANG/USE_COMPILER\t\t= GCC/' config.mk
make -j$(nproc)
```

The resulting binary is placed under `build/mt-api`. Static assets are generated
into `build/src/css` and `src/web/...`. Use `make install DESTDIR=/opt/api.dist`
if you want to copy everything into a staging directory.

## Configuration & runtime

1. Copy `sqlpasswd` to `/opt/api/data/.passwd/sqlpasswd` and store the MariaDB
   user with read access to the Mediathek database.
2. `mt-api.cgi` expects `DOCUMENT_ROOT` to be set by your web server and will
   execute `/opt/api/bin/mt-api`.
3. FastCGI example:
   ```bash
   spawn-fcgi -s /run/mt-api.sock -u www-data -g www-data /opt/api/bin/mt-api
   ```
4. The bundled `docker/api/lighttpd.conf` serves as a reference lighttpd setup.

## Development & testing

- `make clean && make` – rebuild
- `make css` – regenerate only the SCSS/CSS assets
- `make lint` – (WIP) upcoming style checks
- For end-to-end validation run `make smoke` in the parent
  `services/mediathek-backend` directory.

## Versioning

We follow Semantic Versioning. This release is tagged **v0.2.0**, featuring the
plain JSON responses and simplified dependency chain. See `git tag` for
previous versions.

## Support

Please use GitHub Issues / Pull Requests for questions and contributions:
<https://github.com/tuxbox-neutrino/mt-api-dev>.
