#!/bin/bash
# Prueba: servidor con límite MAX_CLIENTES y varios intentos de conexión.
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

PORT=8082
MAX=3      # límite pequeño para la prueba
BACKLOG=10
CSV=data/productos.csv
LOG=test_full_actions.log
mkdir -p scripts/logs
rm -f "$LOG" scripts/logs/* temp.csv server.pid

# arrancar servidor (via run_server.sh para compilar si hace falta)
./scripts/run_server.sh $PORT $MAX $BACKLOG $CSV $LOG 

sleep 1

# función cliente simple que intenta conectar y pedir MOSTRAR
cliente_try() {
  id="$1"
  out="scripts/logs/cliente_full_${id}.out"
  rm -f "$out"
  (
    exec 3<>/dev/tcp/127.0.0.1/$PORT
    if [ $? -ne 0 ]; then
      echo "CONNECT_FAIL" > "$out"
      exit 0
    fi
    # leer saludo en background
    timeout 3 cat <&3 > "$out" & reader=$!
    printf "BEGIN\n" >&3
    sleep 1
    kill "$reader" 2>/dev/null || true
    exec 3>&-; exec 3<&-
  ) &
  echo $!
}

# lanzar más clientes que el límite
PIDS=()
TRIES=20
for i in $(seq 1 $TRIES); do
  pid=$(cliente_try $i)
  PIDS+=($pid)
  sleep 0.2
done

# esperar a todos
sleep 3
wait ${PIDS[@]} 2>/dev/null || true

# extraer indicadores de rechazo/éxito
echo "=== Resultado prueba servidor lleno (logs en scripts/logs) ===" > "$LOG"
for f in scripts/logs/cliente_full_*.out; do
  echo "---- $f ----" >> "$LOG"
  if [ -f "$f" ]; then
    grep -E "ERROR: Servidor ocupado|Servidor ocupado|CONNECT_FAIL" "$f" || true
    cat "$f" >> "$LOG"
  fi
done

echo "Test completo. Log: $LOG"
exec ./scripts/stop_server.sh