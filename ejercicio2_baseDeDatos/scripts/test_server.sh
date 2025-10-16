#!/bin/bash
# Script de prueba básica de concurrencia / transacciones
# Crea logs en scripts/logs/, arranca servidor, lanza 2 clientes y muestra acciones relevantes.
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

LOG_SERVER="test_server.log"
ACTIONS_LOG="test_actions.log"
mkdir -p scripts/logs
rm -f "$LOG_SERVER" "$ACTIONS_LOG" scripts/logs/* temp.csv

# arrancar servidor en background
echo "Arrancando servidor..."
./bin/servidor 127.0.0.1 8080 5 10 data/productos.csv "$LOG_SERVER" >/dev/null 2>&1 &
PID_SRV=$!
sleep 1
echo "Servidor PID=$PID_SRV"

# helper para cliente: envía comandos y captura respuesta
run_client() {
  name="$1"
  shift
  cmds=("$@")
  out="scripts/logs/${name}.out"
  rm -f "$out"
  (
    # abrir socket con /dev/tcp (bash)
    exec 3<>/dev/tcp/127.0.0.1/8080
    # leer respuesta inicial en background hacia archivo
    timeout 10 cat <&3 > "$out" & reader_pid=$!
    # enviar comandos con pausas
    for c in "${cmds[@]}"; do
      printf "%s\n" "$c" >&3
      sleep 1
    done
    # esperar un poco y cerrar
    sleep 1
    kill "$reader_pid" 2>/dev/null || true
    exec 3>&-; exec 3<&-
  ) &
  echo $!
}

# Cliente A: inicia transacción, modifica (usa formato ID;nueva_linea), espera y confirma
cmdsA=("BEGIN" "MODIFICAR 1;1,G1_001_TEST,99,2025-10-16,12:00:00,1" "AGREGAR 999,GX_999,5,2025-10-16,12:00:00,9" "COMMIT" "SALIR")
# Cliente B: intenta operar mientras A tiene lock
cmdsB=("MOSTRAR" "BUSCAR G1_001" "SALIR")

echo "Lanzando clientes..."
run_client "clienteA" "${cmdsA[@]}"
# darle tiempo a que A tome lock antes de B
sleep 1
run_client "clienteB" "${cmdsB[@]}"

# esperar finalización de clientes
sleep 8

# detener servidor
echo "Deteniendo servidor..."
kill "$PID_SRV" 2>/dev/null || true
sleep 1

# filtrar acciones relevantes
PAT='Cliente|Recibido|Transacción|AGREGAR|MODIFICAR|ELIMINAR|MOSTRAR|COMMIT|ROLLBACK|Conectado|Desconectado|Desconectando|ERROR: Existe una transacción activa|Transacción iniciada|Transacción confirmada|Transacción descartada|ERROR: Servidor ocupado'
grep -E "$PAT" "$LOG_SERVER" | tee "$ACTIONS_LOG"

echo "Logs de clientes guardados en scripts/logs/*.out"
echo "Log servidor: $LOG_SERVER"
echo "Acciones extraídas: $ACTIONS_LOG"