#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(pwd)"
CONFIG_DIR="${ROOT_DIR}/config/importer"
DATA_DIR="${ROOT_DIR}/data/importer"
NETWORK_NAME="${NETWORK_NAME:-mediathek-net}"
DB_HOST="${MT_API_DB_HOST:-db}"

mkdir -p "${CONFIG_DIR}" "${DATA_DIR}"
if [[ ! -f "${CONFIG_DIR}/mv2mariadb.conf" ]]; then
  echo "[quickstart] Missing ${CONFIG_DIR}/mv2mariadb.conf"
  exit 1
fi
if [[ ! -f "${CONFIG_DIR}/pw_mariadb" ]]; then
  echo "[quickstart] Missing ${CONFIG_DIR}/pw_mariadb"
  exit 1
fi

docker network inspect "${NETWORK_NAME}" >/dev/null 2>&1 || docker network create "${NETWORK_NAME}"

docker pull dbt1/mediathek-importer:latest
docker pull dbt1/mt-api-dev:latest

docker run --rm \
  -v "${CONFIG_DIR}:/opt/importer/config" \
  -v "${DATA_DIR}:/opt/importer/bin/dl" \
  --network "${NETWORK_NAME}" \
  dbt1/mediathek-importer --update

docker rm -f mediathek-importer >/dev/null 2>&1 || true
docker run -d --name mediathek-importer \
  -v "${CONFIG_DIR}:/opt/importer/config" \
  -v "${DATA_DIR}:/opt/importer/bin/dl" \
  --network "${NETWORK_NAME}" \
  dbt1/mediathek-importer --cron-mode 120 --cron-mode-echo

docker rm -f mediathek-api >/dev/null 2>&1 || true
docker run -d --name mediathek-api \
  --network "${NETWORK_NAME}" \
  -p 18080:8080 \
  -e MT_API_DB_HOST="${DB_HOST}" \
  dbt1/mt-api-dev:latest

echo "[quickstart] Importer and API are running. Visit http://localhost:18080/mt-api?mode=api&sub=info"
