# Neutrino Mediathek API

This repository hosts the backend API consumed by the Neutrino Mediathek plugin.
It exposes lightweight CGI/FastCGI endpoints that render JSON and HTML data
based on the MediathekView catalogue stored in MariaDB.

## Architecture overview

The backend is split into three independent building blocks:

1. **MariaDB** – stores the MediathekView catalogue (`mediathek_1` tables). You
   can reuse an existing database server or let the quickstart script spin up a
   `mariadb:11.4` container.
2. **Importer (`dbt1/mediathek-importer`)** – downloads the movie lists and
   writes them into MariaDB. Run it once with `--update`, then periodically with
   `--cron-mode` so the tables stay current.
3. **API (`dbt1/mt-api-dev`)** – read-only component that exposes the data to
   Neutrino via CGI/FastCGI. It must point to an already-populated MariaDB
   instance via `MT_API_DB_HOST`, user and password.

Because the API container does not embed a database, the typical installation
order is **(1) start MariaDB → (2) run the importer → (3) start the API**. The
`scripts/quickstart.sh` helper automates this sequence on a single host.

## Table of Contents

- [Quickstart Script](#quickstart-script)
- [Highlights](#highlights)
- [Quickstart (Docker Compose)](#quickstart-docker-compose)
- [Prebuilt Docker Image](#prebuilt-docker-image)
- [Requirements](#requirements)
- [Manual Build](#manual-build)
- [Configuration & Runtime](#configuration--runtime)
- [Development & Testing](#development--testing)
- [Versioning](#versioning)
- [Support](#support)

## Quickstart Script

Use this interactive helper when you want one command to provision MariaDB, the
importer and the API containers with sensible defaults.

You can bootstrap importer + API with a single helper script:

```bash
curl -fsSL https://raw.githubusercontent.com/tuxbox-neutrino/mt-api-dev/master/scripts/quickstart.sh -o quickstart.sh
chmod +x quickstart.sh
./quickstart.sh
```

During the run you will be asked for the MariaDB host/user/password (defaults:
`root`/`example-root`). Simply change the password during the prompt.
By default, the script automatically starts a local MariaDB container.
(`mariadb:11.4`), generates the importer config under
`config/importer/` plus the API `sqlpasswd` under `config/api/`, pulls the Docker
images and finally launches long-running importer **and** API containers with
`--restart unless-stopped`.

That's all – there's nothing more to do.

You can override defaults via environment variables before launching, for
example when your MariaDB already runs on the host:

```bash
NETWORK_NAME=bridge MT_API_DB_HOST=192.168.1.50 ./quickstart.sh
```

After the script finishes you will see freshly generated configs inside
`config/importer/` (importer settings), `config/api/` (SQL password) and cached
downloads under `data/importer/`. The importer and API containers keep running
in the background (`--restart unless-stopped`). Verify everything with:

```bash
docker ps --filter name=mediathek
curl http://localhost:18080/mt-api?mode=api&sub=info
docker logs mediathek-importer
docker logs mediathek-api
```

Re-run `./quickstart.sh` whenever you want to regenerate configs or adjust the
network/database parameters; it will reuse the existing folders.

Default values used by the script (change them via env vars or prompts):

- Docker network: `mediathek-net` (bridge mode) or `host` if `NETWORK_MODE=host`
- API port: `18080` on the host
- Container names: `mediathek-db`, `mediathek-importer`, `mediathek-api`
- MariaDB credentials: user `root`, password `example-root`
- MariaDB host: `mediathek-db` (when starting the bundled DB) or your answer to
  the "MariaDB host" prompt

### Credential tips

- The defaults (`root` / `example-root`) are **only meant for local testing**.
  Pick your own `DB_USER`, `DB_PASS` (and optionally `DB_ROOT_PASS`) via env
  variables or when prompted before exposing the service publicly.
- When the script launches the bundled MariaDB container it automatically
  creates the requested user and grants access to `mediathek_*` schemas. For
  external databases it simply writes the credentials into the config and
  assumes the account already exists.
- Use environment overrides such as  
  `DB_USER=mediathek DB_PASS=$(openssl rand -hex 12) DB_ROOT_PASS=myrootpw ./quickstart.sh`
  to provision non-default accounts in one go.
- Config files (`config/importer/pw_mariadb`, `config/api/sqlpasswd`) are
  rewritten on every run so changed passwords immediately take effect.

### Database readiness & troubleshooting

- Raspberry Pi-class devices may need 60–90 s for the bundled MariaDB to finish
  initialisation. The spinner shown after “Waiting for MariaDB …” indicates that
  the script is still polling the container; it will only continue once a ping
  succeeds (or abort after the timeout).
- Right after `--update` the helper automatically runs `mv2mariadb --force-convert`
  once so the database is seeded with the latest movie list. This step can take
  several minutes and downloads ~2 GB of data on the first run.
- If the script had to abort, you can manually resume the same steps:
  1. `docker ps` and `docker logs mediathek-db` to ensure the DB is running.
  2. `docker run --rm … dbt1/mediathek-importer --update` to bootstrap tables.
  3. Start the long-running importer and API containers exactly as shown further
     below.

## Highlights

- REST-ish endpoints (`mode=api&sub=…`) for programme lists, channels and
  livestreams.
- Works with any CGI/FastCGI capable web server (lighttpd, nginx, Apache).
- Ships static HTML/CSS assets alongside the binary.
- No external LLVM / Boost URI dependency anymore – the binary links only
  against libc/pthread/dl.

## Quickstart (Docker Compose)

If you are hacking inside the `neutrino-make` repository, this compose stack
mirrors the production topology and keeps every dependency local.

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

For standalone deployments (Raspberry Pi, VPS, bare metal) you can run the
published OCI image directly.

GitHub Actions builds and pushes a multi-arch image to Docker Hub:
`docker pull dbt1/mt-api-dev:latest`. It contains the compiled binary,
static assets and a lighttpd setup. Point it to an existing MariaDB instance:

```bash
docker run -d --name mt-api \
  -e MT_API_DB_HOST=db \
  -p 18080:8080 \
  dbt1/mt-api-dev:latest
```

Use a host name that matches your scenario:

- `db` – common when a Docker compose stack exposes MariaDB under that service
  name.
- `127.0.0.1` / `localhost` – when the API runs with `--network host` on the
  same machine as MariaDB.
- `192.168.1.50` – typical for a separate VM/NAS in the home network.

To update an existing deployment simply pull the new image and restart:

```bash
docker pull dbt1/mt-api-dev:latest
docker stop mt-api && docker rm mt-api
docker run -d --name mt-api \
  -e MT_API_DB_HOST=192.168.1.50 \
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

Building from source requires the following toolchain and libraries.

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

Compile everything locally with the usual GCC/Clang workflow:

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

Once installed, hook the binary into your web server via CGI/FastCGI:

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

Use the provided helper targets while iterating on the sources:

- `make clean && make` – rebuild
- `make css` – regenerate only the SCSS/CSS assets
- `make lint` – (WIP) upcoming style checks
- For end-to-end validation run `make smoke` in the parent
  `services/mediathek-backend` directory.

## Versioning

We publish tagged releases so the plugin can assert backend compatibility.

We follow Semantic Versioning. This release is tagged **v0.2.0**, featuring the
plain JSON responses and simplified dependency chain. See `git tag` for
previous versions.

## Support

Please use GitHub Issues / Pull Requests for questions and contributions:
<https://github.com/tuxbox-neutrino/mt-api-dev>.
