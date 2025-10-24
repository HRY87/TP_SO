#!/bin/bash
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 1

PIDFILE="./server.pid"

if [ -f "$PIDFILE" ]; then
  PID=$(cat "$PIDFILE" 2>/dev/null)
  if [ -n "$PID" ] && kill -0 "$PID" 2>/dev/null; then
    echo "Deteniendo servidor PID=$PID..."
    kill "$PID"
    # esperar y forzar si no cae
    sleep 1
    if kill -0 "$PID" 2>/dev/null; then
      echo "Forzando kill a PID=$PID"
      kill -9 "$PID"
    fi
  fi
  rm -f "$PIDFILE"
else
  # fallback: intentar por nombre o por puertos de test
  pkill -f "/bin/servidor" 2>/dev/null || true
fi

echo "Servidor detenido (si existía)."