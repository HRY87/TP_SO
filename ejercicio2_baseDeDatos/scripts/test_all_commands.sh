#!/bin/bash
# Ejecuta una sesión completa que prueba TODOS los comandos (single client).
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

PORT=8084
MAX=5
BACKLOG=10
CSV=data/productos.csv
LOG=test_all_actions.log
OUT=scripts/logs/cliente_all.out
mkdir -p scripts/logs
rm -f "$LOG" "$OUT" temp.csv server.pid

chmod +x scripts/run_server.sh
./scripts/run_server.sh $PORT $MAX $BACKLOG $CSV $LOG
sleep 1

# Secuencia de comandos: consultas, DML, transacción, errores esperados
cmds=(
  "BEGIN"
  "MOSTRAR"
  "BUSCAR G1_001"
  "FILTRO 1"
  "AGREGAR 555,GX_555,5,2025-10-16,12:00:00,5"
  "MODIFICAR 555;555,GX_555_MOD,6,2025-10-16,12:00:00,5"
  "ELIMINAR 555"
  "BEGIN"
  "AGREGAR 777,GX_777,7,2025-10-16,12:00:00,7"
  "MODIFICAR 1;1,G1_001_TX,10,2025-10-16,12:00:00,1"
  "COMMIT"
  "ROLLBACK"   # debería indicar que no hay transacción activa
  "SALIR"
)

# ejecutar la sesión
(
  exec 3<>/dev/tcp/127.0.0.1/$PORT || { echo "CONNECT_FAIL" > "$OUT"; exit 0; }
  timeout 2 cat <&3 > "$OUT" & reader=$!
  for c in "${cmds[@]}"; do
    printf "%s\n" "$c" >&3
    sleep 0.5
  done
  sleep 1
  kill "$reader" 2>/dev/null || true
  exec 3>&-; exec 3<&-
)

# guardar resumen
echo "---- Cliente output ----" > "$LOG"
cat "$OUT" >> "$LOG"
echo "Test ALL commands completado. Log: $LOG"

exec ./scripts/stop_server.sh