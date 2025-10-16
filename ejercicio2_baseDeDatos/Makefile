# ===============================================================
# Makefile — Ejercicio 2: Sistema Cliente-Servidor de Base de Datos
# ===============================================================

# --- Compilador y flags ---
CC       = gcc
CFLAGS   = -Iinclude -Wall -Wextra -pthread

# --- Directorios ---
SRC_DIR  = src
BIN_DIR  = bin
DATA_DIR = data
LOG_DIR  = logs
SCRIPTS  = scripts

# --- Archivos fuente ---
SRCS     = $(wildcard $(SRC_DIR)/*.c)
OBJS     = $(SRCS:.c=.o)

# --- Parámetros configurables (se pueden sobreescribir por CLI) ---
IP       ?= 127.0.0.1
PORT     ?= 8080
MAX      ?= 5
BACKLOG  ?= 10
CSV      ?= $(DATA_DIR)/productos.csv
LOG      ?= $(LOG_DIR)/server.log

# ===============================================================
# OBJETIVOS PRINCIPALES
# ===============================================================

all: dirs servidor cliente

dirs:
	@mkdir -p $(BIN_DIR) $(DATA_DIR) $(LOG_DIR)

# --- Compilación del servidor ---
servidor: $(SRC_DIR)/servidor.c $(SRC_DIR)/db.c $(SRC_DIR)/utils.c $(SRC_DIR)/transaction.c
	$(CC) $(CFLAGS) -o $(BIN_DIR)/servidor $^

# --- Compilación del cliente ---
cliente: $(SRC_DIR)/cliente.c $(SRC_DIR)/utils.c
	$(CC) $(CFLAGS) -o $(BIN_DIR)/cliente $^

# ===============================================================
# EJECUCIÓN RÁPIDA
# ===============================================================

# Ejecutar servidor (con parámetros por defecto o pasados por CLI)
run-server: servidor
	@echo "🚀 Iniciando servidor en $(IP):$(PORT)"
	@echo "👉 Ejecutando: $(BIN_DIR)/servidor $(IP) $(PORT) $(MAX) $(BACKLOG) $(CSV) $(LOG)"
	@$(BIN_DIR)/servidor $(IP) $(PORT) $(MAX) $(BACKLOG) $(CSV) $(LOG) &
	@echo "📝 Logs en $(LOG)"


# Ejecutar cliente interactivo (requiere que el servidor esté corriendo)
run-cliente: cliente
	@echo "🧑‍💻 Iniciando cliente conectado a $(IP):$(PORT)"
	@$(BIN_DIR)/cliente $(IP) $(PORT)

# Alias corto
run: run-server

# ===============================================================
# TESTS Y UTILIDADES
# ===============================================================

test: servidor cliente
	chmod +x $(SCRIPTS)/test_concurrency.sh
	$(SCRIPTS)/test_concurrency.sh

test-full: servidor cliente
	chmod +x $(SCRIPTS)/test_server_full.sh
	$(SCRIPTS)/test_server_full.sh

test-many: servidor cliente
	chmod +x $(SCRIPTS)/test_many_clients.sh
	$(SCRIPTS)/test_many_clients.sh

test-all: servidor cliente
	chmod +x $(SCRIPTS)/test_all_commands.sh
	$(SCRIPTS)/test_all_commands.sh

# Detener servidor (si está en segundo plano)
stop-server:
	chmod +x $(SCRIPTS)/stop_server.sh
	$(SCRIPTS)/stop_server.sh

# ===============================================================
# LIMPIEZA
# ===============================================================

clean:
	rm -rf $(BIN_DIR)/*.o \
	       $(BIN_DIR)/servidor \
	       $(BIN_DIR)/cliente \
	       $(LOG) \
	       server_debug.log \
	       test_*.log \
	       $(SCRIPTS)/logs/* \
	       server.pid

# ===============================================================
# PHONY TARGETS
# ===============================================================

.PHONY: all clean dirs servidor cliente \
        run run-server run-cliente \
        test test-full test-many test-all \
        reparar restore-csv stop-server
