# ===========================================================
# Makefile para proyecto "productos"
# ===========================================================

# Valores por defecto (pueden cambiarse desde consola)
GENS ?= 4
TOTAL ?= 100

# Archivos principales
PROG = productos
VALIDADOR = validar.awk
MONITOREO = monitorear.sh
CSV = productos.csv
MLOG = monitoreo.log
PLOG = productos.log
LOG = $(MLOG) $(PLOG)

# Compilación
CC = gcc
CFLAGS = -Wall -pthread -lrt

# ===============================
# Reglas principales
# ===============================

all: $(PROG)

$(PROG): $(PROG).c
	@echo "Compilando $(PROG)..."
	$(CC) $(CFLAGS) -o $(PROG) $(PROG).c
	@echo "Compilación finalizada."

# -------------------------------
# Ejecutar el programa principal
# -------------------------------
run: $(PROG)
	@echo "Ejecutando con $(GENS) generadores y $(TOTAL) registros..."
	./$(PROG) $(GENS) $(TOTAL)
	@echo "Archivo generado: $(CSV)"

# -------------------------------
# Monitoreo con script externo
# -------------------------------
monitorear: $(MONITOREO)
	@echo "Iniciando monitoreo del sistema..."
	./$(MONITOREO) $(GENS) $(TOTAL)

# -------------------------------
# Validación del CSV
# -------------------------------
validar: $(CSV) $(VALIDADOR)
	@echo "Validando archivo $(CSV)..."
	awk -v GENS=$(GENS) -v TOTAL=$(TOTAL) -f $(VALIDADOR) $(CSV)
	@echo "Validación finalizada."

# -------------------------------
# Limpieza de archivos temporales
# -------------------------------
clean:
	@echo "Limpiando archivos temporales..."
	rm -f $(PROG) $(LOG) $(CSV) *.o
	@echo "Limpieza completa."

# -------------------------------
# Ayuda
# -------------------------------
help:
	@echo ""
	@echo "===== AYUDA DEL MAKEFILE ====="
	@echo ""
	@echo "Comandos disponibles:"
	@echo "  make                				-> Compila el programa"
	@echo "  make run 		GENS=X TOTAL=Y 		-> Ejecuta el programa con parámetros"
	@echo "  make monitorear GENS=X TOTAL=Y 	-> Ejecuta el monitoreo del sistema"
	@echo "  make validar GENS=X TOTAL=Y 		-> Valida el archivo CSV"
	@echo "  make clean          				-> Elimina archivos generados"
	@echo ""
	@echo "Ejemplo:"
	@echo "  make run 		GENS=2 TOTAL=50"
	@echo "  make monitorear 	GENS=2 TOTAL=50"
	@echo "  make validar 		GENS=2 TOTAL=50"
	@echo ""
