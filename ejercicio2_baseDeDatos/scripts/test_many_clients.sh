#!/bin/bash
# Prueba de carga/concurrencia: N clientes concurrentes realizando operaciones aleatorias.
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

PORT=8083
MAX=50
BACKLOG=100
CSV=data/productos.csv
LOG=test_many_actions.log
mkdir -p scripts/logs
rm -f "$LOG" scripts/logs/* temp.csv server.pid

./scripts/run_server.sh 127.0.0.1 $PORT $MAX $BACKLOG $CSV $LOG
sleep 1

random_cmd() {
  case $((RANDOM % 6)) in
    0) echo "MOSTRAR" ;;
    1) echo "BUSCAR G1_00$(( (RANDOM % 10) + 1 ))" ;;
    2) echo "FILTRO $(( (RANDOM % 5) + 1 ))" ;;
    3) id=$((1000 + RANDOM % 9000)); echo "AGREGAR $id,GX_$id,$((1 + RANDOM % 50)),2025-10-16,12:00:00,$((1 + RANDOM % 10))" ;;
    4) id=$((1 + RANDOM % 20)); echo "MODIFICAR $id;$id,G1_${id}_MOD,1,2025-10-16,12:00:00,1" ;;
    5) id=$((1 + RANDOM % 20)); echo "ELIMINAR $id" ;;
  esac
}

# lanzamos N clientes que envían M comandos cada uno
N=20
M=10
PIDS=()
for i in $(seq 1 $N); do
  out="scripts/logs/cliente_many_${i}.out"
  rm -f "$out"
  (
    exec 3<>/dev/tcp/127.0.0.1/$PORT || { echo "CONNECT_FAIL" > "$out"; exit 0; }
    timeout 1 cat <&3 > "$out" & reader=$!
    for j in $(seq 1 $M); do
      cmd=$(random_cmd)
      printf "%s\n" "$cmd" >&3
      sleep 0.1
    done
    sleep 0.5
    kill "$reader" 2>/dev/null || true
    exec 3>&-; exec 3<&-
  ) &
  PIDS+=($!)
  sleep 0.02
done

# esperar y reunir resultados
wait ${PIDS[@]} 2>/dev/null || true
echo "=== Resultados many clients ===" > "$LOG"
for f in scripts/logs/cliente_many_*.out; do
  echo "---- $f ----" >> "$LOG"
  head -n 20 "$f" >> "$LOG"
done

echo "Test many clients finalizado. Log: $LOG"