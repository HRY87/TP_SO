#!/bin/bash
# ===========================================================
# Script de MONITOREO DEL SISTEMA CONCURRENTE "productos"
# ===========================================================
# Evidencia:
#   - Creación/finalización de procesos
#   - Uso de memoria compartida y semáforos POSIX (en /dev/shm)
#   - Recursos IPC SysV (ipcs)
#   - Concurrencia y limpieza final
# ===========================================================

#!/bin/bash
# ===========================================================
# Script de MONITOREO DEL SISTEMA CONCURRENTE "productos"
# ===========================================================
# Evidencia:
#   - Creación/finalización de procesos
#   - Uso de memoria compartida y semáforos POSIX (/dev/shm)
#   - Recursos IPC SysV (ipcs)
#   - Concurrencia y limpieza final
# ===========================================================

GENS=${1:-4}
TOTAL=${2:-100}
TARGET=./productos
CSV=productos.csv
LOG=monitoreo.log
LOG_RUN=productos.log
TMP_BEFORE=/tmp/ipc_before.txt
TMP_DURING=/tmp/ipc_during.txt
TMP_AFTER=/tmp/ipc_after.txt

echo "===== MONITOREO DE PRODUCTOS ====="
date
echo "Generadores: $GENS | Registros totales: $TOTAL"
echo "Salida CSV: $CSV"
echo "------------------------------------"
echo "" | tee "$LOG"

# -----------------------------------------------------------
# VALIDACIÓN DE EJECUTABLE
# -----------------------------------------------------------
if [ ! -x "$TARGET" ]; then
    echo "❌ No se encontró el ejecutable '$TARGET'. Compila antes de ejecutar." | tee -a "$LOG"
    exit 1
fi

# -----------------------------------------------------------
# FUNCIÓN AUXILIAR: CAPTURAR ESTADO DE IPC
# -----------------------------------------------------------
capture_ipc() {
    local label="$1"
    local outfile="$2"

    echo ">>> [$label] Memoria compartida POSIX (/dev/shm)" | tee -a "$LOG"
    ls -lh /dev/shm | tee -a "$outfile" "$LOG"
    echo "" | tee -a "$LOG"

    echo ">>> [$label] Recursos IPC SysV (ipcs)" | tee -a "$LOG"
    ipcs -m -s | grep "$(whoami)" | tee -a "$outfile" "$LOG"
    echo "---------------------------------------------------" | tee -a "$LOG"
}

# ===========================================================
# ESTADO INICIAL
# ===========================================================
echo ">>> [ANTES] Estado inicial del sistema IPC" | tee -a "$LOG"
capture_ipc "ANTES" "$TMP_BEFORE"
echo "" | tee -a "$LOG"

# ===========================================================
# EJECUCIÓN DEL PROGRAMA
# ===========================================================
echo ">>> Iniciando programa..." | tee -a "$LOG"
"$TARGET" "$GENS" "$TOTAL" > "$LOG_RUN" 2>&1 &
PID=$!
sleep 1

echo ">>> PID principal: $PID" | tee -a "$LOG"
echo "" | tee -a "$LOG"

echo ">>> [DURANTE] Procesos relacionados con productos" | tee -a "$LOG"
ps -ef | grep productos | grep -v grep | tee -a "$LOG"
echo "" | tee -a "$LOG"

echo ">>> Árbol de procesos (pstree -p $PID)" | tee -a "$LOG"
pstree -p "$PID" | tee -a "$LOG"
echo "" | tee -a "$LOG"

echo ">>> [DURANTE] Estado de IPC durante ejecución" | tee -a "$LOG"
capture_ipc "DURANTE" "$TMP_DURING"

echo ">>> Monitoreo de sistema (vmstat cada 1s x 3)" | tee -a "$LOG"
vmstat 1 3 | tee -a "$LOG"
echo "" | tee -a "$LOG"

# ===========================================================
# FINALIZACIÓN
# ===========================================================
echo ">>> Esperando finalización del proceso PID=$PID..." | tee -a "$LOG"
wait "$PID"
EXIT_CODE=$?

echo "" | tee -a "$LOG"
echo ">>> Programa finalizado con código $EXIT_CODE" | tee -a "$LOG"

if [ $EXIT_CODE -ne 0 ]; then
    echo "⚠️  El programa terminó con código no cero (posible error)." | tee -a "$LOG"
fi

# Mostrar resumen del log de ejecución (no se trata como error)
if [ -f "$LOG_RUN" ]; then
    echo "" | tee -a "$LOG"
    echo ">>> Últimas líneas del registro de ejecución ($LOG_RUN):" | tee -a "$LOG"
    tail -n 15 "$LOG_RUN" | tee -a "$LOG"
    echo "" | tee -a "$LOG"
    echo "📄 Registro completo guardado en: $LOG_RUN" | tee -a "$LOG"
    echo "" | tee -a "$LOG"
fi

# ===========================================================
# ESTADO FINAL
# ===========================================================
echo ">>> [DESPUÉS] Estado final del sistema IPC" | tee -a "$LOG"
capture_ipc "DESPUÉS" "$TMP_AFTER"

echo ">>> Comparando estado de /dev/shm antes y después" | tee -a "$LOG"
diff "$TMP_BEFORE" "$TMP_AFTER" | tee -a "$LOG" || true
echo "" | tee -a "$LOG"

# ===========================================================
# VALIDACIÓN CSV
# ===========================================================
if [ -f "$CSV" ]; then
    echo ">>> Archivo CSV generado correctamente." | tee -a "$LOG"
    ls -lh "$CSV" | tee -a "$LOG"
    echo "" | tee -a "$LOG"

    if [ -f validar.awk ]; then
        echo ">>> Ejecutando validación de IDs..." | tee -a "$LOG"
        awk -v GENS="$GENS" -v TOTAL="$TOTAL" -f validar.awk "$CSV" | tee -a "$LOG"
    else
        echo "⚠️  No se encontró validar.awk. Saltando validación." | tee -a "$LOG"
    fi
else
    echo "❌ No se generó el archivo $CSV" | tee -a "$LOG"
fi
echo "" | tee -a "$LOG"

# ===========================================================
# LIMPIEZA DE RECURSOS IPC
# ===========================================================
echo ">>> Limpieza de recursos IPC huérfanos..." | tee -a "$LOG"

# Limpiar POSIX shm
for file in /dev/shm/*; do
    if [[ "$file" == *productos* ]]; then
        echo "🧹 Eliminando $file" | tee -a "$LOG"
        rm -f "$file"
    fi
done

# Limpiar SysV IPC
for seg in $(ipcs -m | awk '/'"$(whoami)"'/ {print $2}'); do
    ipcrm -m "$seg" 2>/dev/null && echo "🧹 Eliminado segmento $seg" | tee -a "$LOG"
done
for sem in $(ipcs -s | awk '/'"$(whoami)"'/ {print $2}'); do
    ipcrm -s "$sem" 2>/dev/null && echo "🧹 Eliminado semáforo $sem" | tee -a "$LOG"
done

echo "" | tee -a "$LOG"
echo "===== MONITOREO FINALIZADO =====" | tee -a "$LOG"
date | tee -a "$LOG"
echo "------------------------------------"
echo "Resultados guardados en: $LOG"
echo "------------------------------------"
