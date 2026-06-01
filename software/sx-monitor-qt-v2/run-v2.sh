#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="$ROOT_DIR/build-v2/app-v2/sx_monitor_qt_v2"

if [[ ! -x "$BIN" ]]; then
  echo "Fehler: V2-Binary nicht gefunden oder nicht ausführbar: $BIN" >&2
  echo "Bitte zuerst bauen: cmake -S . -B build-v2 -DCMAKE_BUILD_TYPE=Release && cmake --build build-v2 -j4 --target sx_monitor_qt_v2" >&2
  exit 1
fi

exec "$BIN" "$@"
