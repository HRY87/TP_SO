# ========================================================
# Makefile - Ejercicio 1 Sistemas Operativos
# Generador de datos con memoria compartida y semáforos
# ========================================================

# Variables
CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -O2
TARGET  = sensores
SRC     = sensores.c
OUT     = salida.csv

# ========================================================
# Reglas principales
# ========================================================

# Compilar el programa
build: $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) -lrt -pthread

# Ejecutar con parámetros de ejemplo
run: build
	./$(TARGET) 4 50

# Validar salida con AWK
check: run
	@echo "\n--- Validando CSV ---"
	awk -f validar.awk $(OUT)

# Limpieza de binarios, CSV y recursos IPC
clean:
	rm -f $(TARGET) $(OUT)
	-@ipcrm -M 0x123456 2>/dev/null || true
	-@ipcrm -S 0x123456 2>/dev/null || true
	@echo "Limpieza completa."

# ========================================================
# Ayuda
# ========================================================
help:
	@echo "Comandos disponibles:"
	@echo "  make build   -> Compilar el programa"
	@echo "  make run     -> Ejecutar ejemplo (4 generadores, 50 registros)"
	@echo "  make check   -> Ejecutar y validar salida con validar.awk"
	@echo "  make clean   -> Limpiar binarios, CSV y recursos IPC"
