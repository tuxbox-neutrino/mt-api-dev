# Neutrino Mediathek API

This repository hosts the backend API consumed by the Neutrino Mediathek plugin.
It exposes lightweight CGI/FastCGI endpoints that render JSON and HTML data
based on the MediathekView catalogue stored in MariaDB.

## Quickstart Script

You can bootstrap importer + API with a single helper script:

```bash
curl -fsSL https://raw.githubusercontent.com/tuxbox-neutrino/mt-api-dev/master/scripts/quickstart.sh -o quickstart.sh
chmod +x quickstart.sh
./quickstart.sh
```

The script expects `config/importer/mv2mariadb.conf` and `config/importer/pw_mariadb`
in the working directory (create `config/importer` and drop your existing files there).
It pulls the Docker images, runs the importer once with `--update`, then starts
long-running importer and API containers. You can override defaults via env vars:

```bash
NETWORK_NAME=my-net MT_API_DB_HOST=db.example.org ./quickstart.sh
```

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
`docker pull dbt1/mt-api-dev:latest`. It contains the compiled binary,
static assets and a lighttpd setup. Point it to an existing MariaDB instance:

```bash
docker run -d --name mt-api \
  -e MT_API_DB_HOST=db.example.org \
  -p 18080:8080 \
  dbt1/mt-api-dev:latest
```

To update an existing deployment simply pull the new image and restart:

```bash
docker pull dbt1/mt-api-dev:latest
docker stop mt-api && docker rm mt-api
docker run -d --name mt-api \
  -e MT_API_DB_HOST=db.example.org \
  -p 18080:8080 \
  dbt1/mt-api-dev:latest
```

Persist `/opt/api/data` and `/opt/api/log` via volumes if you want to keep
templates/logs across restarts. See `.github/workflows/docker-publish.yml`
for the automated build definition.

Typical RasPi/PC setup together with the importer:

```bash
# run everything via helper script
# (creates config/data directories relative to current working dir)
./scripts/quickstart.sh

# MariaDB reachable under network name "db" (docker compose or host IP)
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

> Tip: replace `mediathek-net` with any existing Docker network (compose
> stack). If you want to access a database on the host directly, use
> `--network host`. Drop `--rm` and add `--name mediathek-importer` when you
> want a long-running container that you can inspect via `docker logs`.

docker run -d --name mt-api \
  --network mediathek-net \
  -p 18080:8080 \
  -e MT_API_DB_HOST=db \
  dbt1/mt-api-dev:latest
```

All images are built as multi-architecture OCI images, so the same commands
work on amd64 PCs and arm64 Raspberry Pis.

The environment variable `MT_API_DB_HOST` must point to the MariaDB instance
that contains the imported Mediathek tables (host name or IP). When you run the
API via `--network host`, use `MT_API_DB_HOST=localhost` or the actual IP
address of your database server.

For importer details see the README in the companion repository
`tuxbox-neutrino/db-import` (local checkout under `services/mediathek-backend/vendor/db-import/README.md`).

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

> **Version embedding**  
> The build system automatically injects `git describe --tags` into the binary.
> Override it if you build from a tarball:  
> `MT_API_VERSION=0.5.0 make`. The value shows up as `apiversion` in
> `/mt-api?mode=api&sub=info`.

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
