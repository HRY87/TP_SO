
# Servidor de Calculos Concurrente (Sockets + Threads)

Un servidor multi-hilo recibe peticiones de hasta 5 clientes que envian expresiones aritmeticas elementales: suma, resta, multiplicacion y division (`+`, `-`, `*`, `/`) enviadas por clientes a traves de sockets TCP. Cada cliente se atiende en un **hilo independiente**. Al cerrar el servidor, se genera un archivo por tipo de operacion con el historial.


---

##  Compilacion

Requiere `gcc` y `pthread`.

### En Linux/macOS:

```bash
gcc servidor.c -o servidor -lpthread
gcc cliente.c -o cliente
```

---

## Ejecucion

### 1. Iniciar el servidor:

```bash
./servidor
```

- Escucha en `localhost:8080`.
- Espera conexiones de hasta 5 clientes concurrentes.
- Para finalizar, presionar **Enter** en la terminal del servidor.
- Al cerrarse, se generan los archivos `sumas.txt`, `restas.txt`, etc.

---

### 2. Ejecutar un cliente (en otra terminal):

```bash
./cliente
```

- Ingresar operaciones como:
  ```
  5 + 3
  12 / 4
  10 * 5
  ```
- Escribir `salir` para desconectarse del servidor.

---

##  Monitoreo del servidor (threads y sockets)

Podes utilizar comandos estandar de Bash para visualizar la concurrencia y gestion de conexiones:

### 1. Ver el PID del servidor

```bash
ps aux | grep servidor
```

### 2. Ver threads del servidor

```bash
ps -T -p <PID>
```

O en tiempo real:

```bash
watch -n 1 "ps -T -p <PID>"
```

### 3. Ver conexiones activas al puerto 8080

```bash
netstat -anp tcp | grep 8080
```

O usando `lsof`:

```bash
lsof -i :8080
```

### 4. Monitor de procesos

- Con `top`:

```bash
top -pid <PID>
```

- Con `htop`:

```bash
htop
```

En `htop`, presionar `H` para ver los threads y `F3` para buscar por nombre o PID.

---

##  Ejemplo de uso

1. Se ejecuta el servidor una vez y tenemos el mensaje:

```
Servidor de calculo iniciado. Presione Enter para finalizar.
```

2. Se ejecutan varios clientes, se pueden enviar operaciones como:

```
5 + 3
10 / 0
salir
```

3. Al cerrar el servidor, se generan archivos como `sumas.txt`:

```txt
5 + 3 = 8.00
```

---

##  Notas

- Las operaciones invalidas o divisiones por cero son rechazadas con un mensaje de error.
- Se usa un `mutex` para proteger el acceso concurrente a los buffers de operaciones.

---

