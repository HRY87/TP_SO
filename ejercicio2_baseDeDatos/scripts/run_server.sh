#!/bin/bash
# Arranca el servidor con valores por defecto (ejecuta make si hace falta).
# Uso: ./run_server.sh [IP] [PORT] [MAX_CLIENTES] [BACKLOG] [CSV_PATH] [LOG_PATH]

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 1

IP=${1:-127.0.0.1}
PORT=${2:-8080}
MAX=${3:-5}
BACKLOG=${4:-10}
CSV=${5:-data/productos.csv}
LOG=${6:-server.log}

BIN="./bin/servidor"
PIDFILE="./server.pid"

# Compilar si no existe binario
if [ ! -x "$BIN" ]; then
  echo "Binario no encontrado. Compilando..."
  make >/dev/null || { echo "make falló"; exit 1; }
fi

# Si ya hay PID y proceso corriendo, avisar
if [ -f "$PIDFILE" ]; then
  OLD_PID=$(cat "$PIDFILE" 2>/dev/null)
  if [ -n "$OLD_PID" ] && kill -0 "$OLD_PID" 2>/dev/null; then
    echo "Servidor ya corriendo (PID=$OLD_PID). Use stop_server.sh o kill $OLD_PID"
    exit 0
  else
    rm -f "$PIDFILE"
  fi
fi

# Asegurar carpeta de logs
touch "$LOG" || { echo "No se puede escribir en $LOG"; exit 1; }

echo "Iniciando servidor en $IP:$PORT (max=$MAX backlog=$BACKLOG) CSV=$CSV LOG=$LOG"
nohup "$BIN" "$IP" "$PORT" "$MAX" "$BACKLOG" "$CSV" "$LOG" >> "$LOG" 2>&1 &
PID=$!
echo $PID > "$PIDFILE"
sleep 1

if kill -0 "$PID" 2>/dev/null; then
  echo "Servidor arrancado (PID=$PID). Log: $LOG"
else
  echo "Fallo al arrancar servidor. Revisa $LOG"
  rm -f "$PIDFILE"
  exit 1
fi