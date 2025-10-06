CC = gcc
CFLAGS = -Wall -g
TARGETS = servidor cliente

# Parámetros de servidor
IP_SERVIDOR = 127.0.0.1
PUERTO = 8080
MAX_CLIENTES = 2
MAX_ESPERA = 3

all: $(TARGETS)

servidor: servidor.c
	$(CC) $(CFLAGS) -o servidor servidor.c

cliente: cliente.c
	$(CC) $(CFLAGS) -o cliente cliente.c

run-servidor: servidor
	./servidor $(IP_SERVIDOR) $(PUERTO) $(MAX_CLIENTES) $(MAX_ESPERA)

# ---- Arranque de varios clientes ----
run-cliente1: cliente
	./cliente $(IP_SERVIDOR) $(PUERTO)

run-cliente2: cliente
	./cliente $(IP_SERVIDOR) $(PUERTO)

run-cliente3: cliente
	./cliente $(IP_SERVIDOR) $(PUERTO)

run-cliente4: cliente
	./cliente $(IP_SERVIDOR) $(PUERTO)

run-cliente5: cliente
	./cliente $(IP_SERVIDOR) $(PUERTO)

run-cliente6: cliente
	./cliente $(IP_SERVIDOR) $(PUERTO)

# Ejecuta 1 servidor y 5 clientes (para probar cuando max_clientes=2 y max_espera=3)
test-servidor-parcial: run-servidor run-cliente1 run-cliente2 run-cliente3
test-servidor-completo: run-servidor run-cliente1 run-cliente2 run-cliente3 run-cliente4 run-cliente5
test-servidor-lleno: run-servidor run-cliente1 run-cliente2 run-cliente3 run-cliente4 run-cliente5 run-cliente6
clean:
	rm -f $(TARGETS) *.o
