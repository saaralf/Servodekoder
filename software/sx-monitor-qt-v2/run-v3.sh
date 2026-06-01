#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="$ROOT_DIR/build-v3/app-v3/sx_monitor_qt_v3"

if [[ ! -x "$BIN" ]]; then
  echo "Fehler: V3-Binary nicht gefunden oder nicht ausführbar: $BIN" >&2
  echo "Bitte zuerst bauen: cmake -S . -B build-v3 -DCMAKE_BUILD_TYPE=Release && cmake --build build-v3 -j4 --target sx_monitor_qt_v3" >&2
  exit 1
fi

exec "$BIN" "$@"
