#!/usr/bin/env bash
set -euo pipefail

if ! command -v docker >/dev/null 2>&1; then
  echo "[quickstart] docker is required."
  exit 1
fi

ROOT_DIR="${ROOT_DIR:-$(pwd)}"
CONFIG_DIR="${CONFIG_DIR:-${ROOT_DIR}/config/importer}"
DATA_DIR="${DATA_DIR:-${ROOT_DIR}/data/importer}"
API_CONFIG_DIR="${API_CONFIG_DIR:-${ROOT_DIR}/config/api}"
NETWORK_MODE="${NETWORK_MODE:-bridge}" # bridge | host
NETWORK_NAME="${NETWORK_NAME:-mediathek-net}"
API_PORT="${API_PORT:-18080}"

mkdir -p "${CONFIG_DIR}" "${DATA_DIR}" "${API_CONFIG_DIR}"

prompt() {
  local __var="$1" __text="$2" __default="$3" __input
  read -r -p "${__text} [${__default}]: " __input
  if [[ -z "${__input}" ]]; then
    __input="${__default}"
  fi
  printf -v "${__var}" '%s' "${__input}"
}

prompt_secret() {
  local __var="$1" __text="$2" __default="$3" __input
  read -r -s -p "${__text} [${__default}]: " __input
  echo
  if [[ -z "${__input}" ]]; then
    __input="${__default}"
  fi
  printf -v "${__var}" '%s' "${__input}"
}

DB_USER="${DB_USER:-root}"
DB_PASS="${DB_PASS:-example-root}"
prompt DB_USER "MariaDB user" "${DB_USER}"
prompt_secret DB_PASS "MariaDB password" "${DB_PASS}"

start_db_default="Y"
read -r -p "Start bundled MariaDB container? [Y/n]: " START_DB
START_DB=${START_DB:-${start_db_default}}

if [[ "${START_DB,,}" =~ ^(y|)$ ]]; then
  DB_HOST="mediathek-db"
else
  prompt DB_HOST "MariaDB host" "${DB_HOST:-localhost}"
fi

API_DB_HOST="${MT_API_DB_HOST:-${DB_HOST}}"

if [[ "${NETWORK_MODE}" == "host" ]]; then
  IMPORTER_NET_ARGS=(--network host)
  API_NET_ARGS=(--network host)
  DB_NET_ARGS=(--network host)
  API_PORT_ARGS=()
else
  docker network inspect "${NETWORK_NAME}" >/dev/null 2>&1 || docker network create "${NETWORK_NAME}"
  IMPORTER_NET_ARGS=(--network "${NETWORK_NAME}")
  API_NET_ARGS=(--network "${NETWORK_NAME}")
  DB_NET_ARGS=(--network "${NETWORK_NAME}")
  API_PORT_ARGS=(-p "${API_PORT}:8080")
fi

if [[ ! -f "${CONFIG_DIR}/mv2mariadb.conf" ]]; then
  cat > "${CONFIG_DIR}/mv2mariadb.conf" <<EOF
aktFileName=Filmliste-akt.xz
diffFileName=Filmliste-diff.xz
downloadServerCount=1
downloadServer_01=https://liste.mediathekview.de/Filmliste-akt.xz
downloadServerConnectFailsMax=3
downloadServerConnectFail_01=0
serverListUrl=https://res.mediathekview.de/akt.xml
serverListRefreshDays=1
mysqlHost=${DB_HOST}
passwordFile=pw_mariadb
videoDb=mediathek_1
videoDbBaseName=mediathek_1
videoDbTemplate=mediathek_1_template
videoDbTmp1=mediathek_1_tmp1
videoDb_TableInfo=channelinfo
videoDb_TableVersion=version
videoDb_TableVideo=video
EOF
  echo "[quickstart] Generated ${CONFIG_DIR}/mv2mariadb.conf"
fi

if [[ ! -f "${CONFIG_DIR}/pw_mariadb" ]]; then
  printf '%s:%s\n' "${DB_USER}" "${DB_PASS}" > "${CONFIG_DIR}/pw_mariadb"
  chmod 600 "${CONFIG_DIR}/pw_mariadb"
  echo "[quickstart] Generated ${CONFIG_DIR}/pw_mariadb"
fi

if [[ ! -f "${API_CONFIG_DIR}/sqlpasswd" ]]; then
  printf '%s:%s\n' "${DB_USER}" "${DB_PASS}" > "${API_CONFIG_DIR}/sqlpasswd"
  chmod 600 "${API_CONFIG_DIR}/sqlpasswd"
  echo "[quickstart] Generated ${API_CONFIG_DIR}/sqlpasswd"
fi

docker pull dbt1/mediathek-importer:latest
docker pull dbt1/mt-api-dev:latest

if [[ "${START_DB,,}" =~ ^(y|)$ ]]; then
  docker pull mariadb:11.4
  docker rm -f mediathek-db >/dev/null 2>&1 || true
  docker run -d --name mediathek-db \
    "${DB_NET_ARGS[@]}" \
    -e MARIADB_ROOT_PASSWORD="${DB_PASS}" \
    -e MARIADB_DATABASE=mediathek_1 \
    --restart unless-stopped \
    mariadb:11.4 \
    --character-set-server=utf8mb4 \
    --collation-server=utf8mb4_unicode_ci
  echo "[quickstart] Started local MariaDB container 'mediathek-db'"
  DB_HOST="mediathek-db"
  API_DB_HOST="${DB_HOST}"
  sed -i "s/^mysqlHost=.*/mysqlHost=${DB_HOST}/" "${CONFIG_DIR}/mv2mariadb.conf"
fi

docker run --rm \
  -v "${CONFIG_DIR}:/opt/importer/config" \
  -v "${DATA_DIR}:/opt/importer/bin/dl" \
  "${IMPORTER_NET_ARGS[@]}" \
  dbt1/mediathek-importer --update

docker rm -f mediathek-importer >/dev/null 2>&1 || true
docker run -d --name mediathek-importer \
  -v "${CONFIG_DIR}:/opt/importer/config" \
  -v "${DATA_DIR}:/opt/importer/bin/dl" \
  "${IMPORTER_NET_ARGS[@]}" \
  --restart unless-stopped \
  dbt1/mediathek-importer --cron-mode 120 --cron-mode-echo

docker rm -f mediathek-api >/dev/null 2>&1 || true
docker run -d --name mediathek-api \
  "${API_NET_ARGS[@]}" \
  "${API_PORT_ARGS[@]:-}" \
  -v "${API_CONFIG_DIR}:/opt/api/config" \
  -e MT_API_DB_HOST="${API_DB_HOST}" \
  --restart unless-stopped \
  dbt1/mt-api-dev:latest

echo "[quickstart] Setup complete. Importer + API are running."
if [[ "${NETWORK_MODE}" == "host" ]]; then
  echo "[quickstart] Access the API via http://localhost:8080/mt-api?mode=api&sub=info"
else
  echo "[quickstart] Access the API via http://localhost:${API_PORT}/mt-api?mode=api&sub=info"
fi
