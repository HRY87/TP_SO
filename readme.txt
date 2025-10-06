README - Sistema Concurrente de Generación de Productos

RESUMEN DEL PROGRAMA

Este proyecto implementa un sistema concurrente en lenguaje C que utiliza procesos, memoria compartida y semáforos POSIX para generar registros de productos en paralelo.
Un proceso coordinador (padre) administra la memoria compartida y varios procesos generadores (hijos) producen registros que se almacenan en un archivo CSV final llamado productos.csv.

El objetivo es demostrar el uso de IPC (Inter Process Communication) mediante sincronización con semáforos y acceso concurrente controlado a una región de memoria compartida.

REGISTRO UTILIZADO

Cada registro representa un producto generado por uno de los procesos hijos y se guarda en formato CSV con los siguientes campos:

ID, Descripcion, Cantidad, Fecha, Hora

Ejemplo:
1, G1_001, 45, 2025-10-06, 15:20:45

ID: número único global asignado secuencialmente.

Descripcion: identificador del generador (G1, G2, etc.) y número interno.

Cantidad: valor aleatorio entre 1 y 50.

Fecha y Hora: obtenidas del sistema en el momento de creación.

SEMÁFOROS UTILIZADOS

Se emplean semáforos POSIX para sincronizar el acceso a los recursos compartidos entre procesos:

sem_ids: protege el contador global de IDs y las variables de control.
Valor inicial: 1

sem_vacio: indica que el buffer está libre para que un generador escriba.
Valor inicial: 1

sem_lleno: indica que hay un registro disponible para leer por el coordinador.
Valor inicial: 0

Los semáforos se nombran dinámicamente usando el PID del proceso padre para evitar colisiones entre ejecuciones simultáneas.

IPC UTILIZADO

Se usa memoria compartida POSIX (shm_open) como medio de comunicación entre procesos.
La estructura compartida (MemCompartida) contiene:

siguiente_id: próximo ID global a asignar.

total: cantidad total de registros a generar.

escritos: cantidad ya escrita.

producers_alive: número de procesos generadores activos.

buffer: estructura que contiene el último producto generado.

El coordinador (padre) lee del buffer y guarda los registros en el archivo CSV.
Los procesos hijos (generadores) escriben productos en el buffer de manera sincronizada.

ARCHIVOS PRINCIPALES

productos.c
Programa principal. Crea los procesos generadores, la memoria compartida y los semáforos.
Coordina la escritura de registros en productos.csv.

monitorear.sh
Script de monitoreo. Ejecuta el programa y guarda información del sistema, procesos e IPC en monitoreo.log.

validar.awk
Script que verifica la integridad de productos.csv, comprobando cantidad total de registros y secuencia correcta por generador.

Makefile
Facilita la compilación, ejecución, monitoreo y validación con comandos simples.

REQUISITOS Y EJECUCIÓN

Requisitos:

Sistema operativo Linux con soporte POSIX.

Paquetes necesarios: gcc, make, gawk, psmisc, procps, util-linux.

Instalación recomendada:
sudo apt update
sudo apt install build-essential gawk psmisc procps util-linux

Opcional (para monitoreo avanzado):
sudo apt install strace pstree

Compilación:
make

Ejecución:
./productos <num_generadores> <total_productos>

Ejemplo:
./productos 2 50

Con Makefile:
make run GENS=2 TOTAL=50

MONITOREO EN LINUX

El script monitorear.sh utiliza herramientas del sistema para observar el estado del programa durante su ejecución:

ps, top, pstree: monitoreo de procesos.

ipcs, lsof: monitoreo de recursos IPC.

strace (opcional): seguimiento de llamadas al sistema.

date, uptime: registro temporal y carga del sistema.

La información se guarda en monitoreo.log para análisis posterior.

COSAS A REVISAR / POSIBLES ERRORES

Que los recursos IPC (memoria y semáforos) se liberen correctamente al finalizar.

Evitar ejecuciones simultáneas sin limpiar previamente (usar make clean).

Revisar permisos en /dev/shm si aparecen errores de acceso.

Validar que productos.csv tenga el número correcto de registros.

Comprobar que la secuencia de IDs sea continua y sin duplicados.

En caso de interrupción o error, limpiar recursos con:
make clean

LIMPIEZA

Para eliminar archivos y recursos IPC generados:

make clean

Esto elimina:

productos (ejecutable)

productos.csv

monitoreo.log

strace.log

Recursos compartidos en /dev/shm y semáforos POSIX asociados