# ===========================
# Makefile Cliente-Servidor Empleados
# ===========================

CC = gcc
CFLAGS = -Wall -pthread
TARGETS = servidor cliente

all: $(TARGETS)

servidor: servidor.c
	$(CC) $(CFLAGS) -o servidor servidor.c

cliente: cliente.c
	$(CC) $(CFLAGS) -o cliente cliente.c

clean:
	rm -f $(TARGETS)

# ---------------------------
# Ejecuciones rápidas
# ---------------------------

# Servidor en localhost:5000, max 5 clientes, 10 en espera
run-servidor:
	./servidor 127.0.0.1 5000 5 10

# Un cliente
run-cliente:
	./cliente 127.0.0.1 5000

# Varios clientes simultáneos
clientes:
	./cliente 127.0.0.1 5000 & \
	./cliente 127.0.0.1 5000 & \
	./cliente 127.0.0.1 5000 & \
	wait

# Matar todos
killall:
	-killall servidor cliente 2>/dev/null || true
