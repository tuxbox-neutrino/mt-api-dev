FROM debian:12-slim AS build

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      bash \
      build-essential \
      pkg-config \
      git \
      ca-certificates \
      python3 \
      libmariadb-dev \
      libcurl4-openssl-dev \
      libfcgi-dev \
      libssl-dev \
      rapidjson-dev \
      libtidy-dev \
      libjsoncpp-dev \
      libboost-system-dev \
      sassc && \
    rm -rf /var/lib/apt/lists/*

RUN useradd -m -s /bin/bash builder

RUN cat <<'EOF' >/usr/local/bin/sass && chmod +x /usr/local/bin/sass
#!/usr/bin/env bash
set -euo pipefail

style="compressed"
input=""
output=""

while (($#)); do
  case "$1" in
    --sourcemap=none|--default-encoding=*)
      shift
      ;;
    --style=*)
      style="${1#*=}"
      shift
      ;;
    *)
      if [[ -z "$input" ]]; then
        input="$1"
      elif [[ -z "$output" ]]; then
        output="$1"
      else
        echo "[sass-wrapper] Unexpected argument: $1" >&2
        exit 1
      fi
      shift
      ;;
  esac
done

if [[ -z "$input" || -z "$output" ]]; then
  echo "Usage: sass [options] input.scss output.css" >&2
  exit 1
fi

sassc --style "$style" "$input" "$output"
EOF

WORKDIR /opt/src/mt-api-dev

COPY . /opt/src/mt-api-dev
RUN chown -R builder:builder /opt/src

USER builder

RUN cp doc/config.mk.sample config.mk && \
    sed -i 's/USE_COMPILER\t\t= CLANG/USE_COMPILER\t\t= GCC/' config.mk && \
    sed -i 's/COMPILER_VER\t= clang50/COMPILER_VER\t= gcc/' config.mk && \
    sed -i 's/COMPILER_VER\t= gcc7/COMPILER_VER\t= gcc/' config.mk && \
    sed -i 's/ENABLE_SANITIZER\t= 1/ENABLE_SANITIZER\t= 0/' config.mk && \
    echo 'STDC++ = -std=c++14' >> config.mk && \
    echo 'EXTRA_CXXFLAGS += -Wno-error=ignored-qualifiers' >> config.mk && \
    bash -lc 'set -euo pipefail; \
      echo "root:example-root" > sqlpasswd; \
      SASS=/usr/local/bin/sass EXTRA_INCLUDES="-I/usr/include/jsoncpp" make -j"$(nproc)"'

USER root

RUN mkdir -p /opt/pkg/www /opt/pkg/bin /opt/pkg/data/.passwd && \
    cp -a build/mt-api /opt/pkg/bin/mt-api && \
    cp -a build/src/css /opt/pkg/www/ && \
    cp -a src/web/www/. /opt/pkg/www/ && \
    cp -a src/web/data/. /opt/pkg/data/ && \
    cp sqlpasswd /opt/pkg/data/.passwd/sqlpasswd && \
    cp docker/mt-api.cgi /opt/pkg/www/mt-api.cgi && \
    chmod +x /opt/pkg/www/mt-api.cgi && \
    chown -R root:root /opt/pkg

FROM debian:12-slim

ENV DEBIAN_FRONTEND=noninteractive \
    API_HOME=/opt/api

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      ca-certificates \
      libmariadb3 \
      libcurl4 \
      libtidy5deb1 \
      libjsoncpp25 \
      libboost-system1.74.0 \
      spawn-fcgi \
      lighttpd && \
    rm -rf /var/lib/apt/lists/*

COPY --from=build /opt/pkg /opt/api.dist
COPY docker/entrypoint.sh /usr/local/bin/api-entrypoint
COPY docker/lighttpd.conf /etc/lighttpd/lighttpd.conf

RUN chmod +x /usr/local/bin/api-entrypoint

EXPOSE 8080

ENTRYPOINT ["/usr/local/bin/api-entrypoint"]
