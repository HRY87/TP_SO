#!/bin/bash
# ===========================================================
# Script de MONITOREO del programa "productos"
# ===========================================================
# Evidencia:
#   - Creación y finalización de procesos
#   - Uso de memoria compartida y semáforos POSIX
#   - Concurrencia de procesos generadores
#   - Limpieza de recursos IPC al finalizar
# ===========================================================

# --- Parámetros de entrada ---
GENS=${1:-4}
TOTAL=${2:-100}
TARGET=./productos
CSV=productos.csv
LOG=monitoreo.log

# --- Inicio ---
echo "===== MONITOREO DE PRODUCTOS ====="
date
echo "Generadores: $GENS, Registros totales: $TOTAL"
echo "Archivo de salida: $CSV"
echo "------------------------------------"
echo "" | tee "$LOG"

# ===========================================================
# ESTADO INICIAL DEL SISTEMA
# ===========================================================
echo ">>> Estado inicial de /dev/shm" | tee -a "$LOG"
ls -lh /dev/shm 2>/dev/null | tee -a "$LOG"
echo "" | tee -a "$LOG"

echo ">>> Procesos previos relacionados con productos" | tee -a "$LOG"
ps -ef | grep productos | grep -v grep | tee -a "$LOG"
echo "" | tee -a "$LOG"

# ===========================================================
# EJECUCIÓN DEL PROGRAMA
# ===========================================================
if command -v strace >/dev/null 2>&1; then
    echo ">>> Ejecutando con strace para registrar llamadas IPC..." | tee -a "$LOG"
    strace -f -e trace=fork,clone,waitpid,sem_open,sem_post,sem_wait,sem_close,shm_open,shm_unlink,open,close,write,read \
        "$TARGET" "$GENS" "$TOTAL" 2> strace.log &
    PID=$!
else
    echo ">>> strace no disponible. Ejecutando sin trazas..." | tee -a "$LOG"
    "$TARGET" "$GENS" "$TOTAL" &
    PID=$!
fi

sleep 1

# ===========================================================
# DURANTE LA EJECUCIÓN
# ===========================================================
echo "" | tee -a "$LOG"
echo ">>> Procesos activos relacionados con productos" | tee -a "$LOG"
ps -ef | grep productos | grep -v grep | tee -a "$LOG"
echo "" | tee -a "$LOG"

echo ">>> Árbol de procesos (pstree -p $PID)" | tee -a "$LOG"
pstree -p "$PID" | tee -a "$LOG"
echo "" | tee -a "$LOG"

echo ">>> Recursos IPC activos en /dev/shm (durante ejecución)" | tee -a "$LOG"
ls -lh /dev/shm 2>/dev/null | tee -a "$LOG"
echo "" | tee -a "$LOG"

echo ">>> Monitoreo del sistema (vmstat 3s)" | tee -a "$LOG"
vmstat 1 3 | tee -a "$LOG"
echo "" | tee -a "$LOG"

# ===========================================================
# FINALIZACIÓN DEL PROCESO
# ===========================================================
echo ">>> Esperando finalización del proceso PID=$PID..." | tee -a "$LOG"
wait "$PID"
EXIT_CODE=$?
echo "" | tee -a "$LOG"
echo ">>> Programa finalizado con código $EXIT_CODE" | tee -a "$LOG"
echo "" | tee -a "$LOG"

# ===========================================================
# RESULTADOS Y VALIDACIÓN
# ===========================================================
if [ -f "$CSV" ]; then
    echo ">>> Archivo CSV generado:" | tee -a "$LOG"
    ls -lh "$CSV" | tee -a "$LOG"
    echo "" | tee -a "$LOG"
else
    echo "No se generó el archivo $CSV" | tee -a "$LOG"
fi

echo ">>> Recursos IPC tras la finalización:" | tee -a "$LOG"
ls -lh /dev/shm 2>/dev/null | tee -a "$LOG"
echo "" | tee -a "$LOG"

# ===========================================================
# VALIDACIÓN AUTOMÁTICA
# ===========================================================
if [ -f validar.awk ]; then
    echo ">>> Ejecutando validación de IDs..." | tee -a "$LOG"
    awk -v GENS="$GENS" -v TOTAL="$TOTAL" -f validar.awk "$CSV" | tee -a "$LOG"
else
    echo "No se encontró validar.awk. Saltando validación." | tee -a "$LOG"
fi

echo "" | tee -a "$LOG"
echo "===== MONITOREO FINALIZADO =====" | tee -a "$LOG"
date | tee -a "$LOG"
echo "------------------------------------"
echo "Recuerda ajustar los parámetros de GENS y TOTAL según el programa compilado."
echo "Ejemplo: make monitorear GENS=2 TOTAL=50"
echo "------------------------------------"
