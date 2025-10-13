# README.txt
# ===========================================================
# Sistema de generación de productos concurrente
# ===========================================================

Este proyecto implementa un sistema concurrente en C para generar registros de productos usando procesos, memoria compartida y semáforos POSIX. Incluye scripts para monitoreo y validación automática del archivo generado.

------------------------------------------------------------
DESCRIPCIÓN GENERAL DEL FUNCIONAMIENTO (productos.c)
------------------------------------------------------------

- El programa principal (`productos.c`) crea un proceso coordinador (padre) y N procesos generadores (hijos).
- Los procesos comparten una estructura en memoria compartida (POSIX SHM) que contiene:
    - Un contador global de IDs (`siguiente_id`)
    - Un buffer único para intercambio de registros (`buffer`)
    - Contadores de registros escritos y generadores vivos
- Los semáforos POSIX se usan para:
    - `sem_ids`: mutex para acceso seguro a los campos críticos de la memoria compartida (IDs, contadores)
    - `sem_vacio`: indica si el buffer está vacío (permite a un generador escribir)
    - `sem_lleno`: indica si el buffer está lleno (permite al coordinador leer)
- Cada generador pide bloques de 10 IDs, genera registros aleatorios y los envía uno por uno al coordinador usando el buffer compartido.
- El coordinador lee los registros del buffer y los almacena en un arreglo temporal, luego los vuelca a un archivo CSV.
- El archivo CSV contiene los campos: ID, Descripción, Cantidad, Fecha, Hora, Generador.
- El sistema maneja señales para limpieza y finalización controlada, y asegura que todos los recursos IPC se liberen al terminar.

------------------------------------------------------------
ARCHIVOS DEL PROYECTO
------------------------------------------------------------

- **productos.c**  
  Código fuente principal en C. Implementa el coordinador y los procesos generadores, usando memoria compartida y semáforos POSIX.  
  Genera el archivo `productos.csv` con los registros.

- **Makefile**  
  Permite compilar, ejecutar, monitorear y validar el sistema fácilmente.  
  Soporta parámetros para cantidad de generadores y registros.

- **monitorear.sh**  
  Script bash para evidenciar la concurrencia, recursos IPC y limpieza de recursos.  
  Ejecuta el programa, monitorea procesos y recursos, y ejecuta la validación automática.

- **validar.awk**  
  Script AWK que valida el archivo `productos.csv` generado:  
  - Verifica que los IDs sean correlativos y únicos  
  - Reporta errores y advertencias  
  - Muestra resumen de generadores y registros

- **productos.csv**  
  Archivo de salida generado por el programa, con los registros de productos.

- **monitoreo.log**  
  Log de monitoreo generado por `monitorear.sh`.

- **productos.log**  
  Salida estándar del programa principal.

------------------------------------------------------------
REGISTROS Y CAMPOS UTILIZADOS EN EL CSV
------------------------------------------------------------

- **ID**: Identificador único y correlativo asignado por el coordinador.
- **Descripcion**: Texto generado por el proceso generador (ej: G1_001).
- **Cantidad**: Número aleatorio entre 1 y 50.
- **Fecha**: Fecha de generación (YYYY-MM-DD).
- **Hora**: Hora de generación (HH:MM:SS).
- **Generador**: Número de generador (1..N) que produjo el registro.

------------------------------------------------------------
COMANDOS DE LINUX USADOS EN monitorear.sh
------------------------------------------------------------

- **ls -lh /dev/shm**  
  Lista los archivos de memoria compartida POSIX activos.
- **ipcs -m -s**  
  Muestra los recursos IPC SysV (memoria y semáforos) activos.
- **ps -ef | grep productos**  
  Muestra los procesos relacionados con el programa.
- **pstree -p <PID>**  
  Muestra el árbol de procesos a partir del PID principal.
- **vmstat 1 3**  
  Monitorea el estado del sistema (CPU, memoria) durante la ejecución.
- **diff <archivo1> <archivo2>**  
  Compara el estado de los recursos IPC antes y después de la ejecución.
- **awk**  
  Ejecuta el script de validación sobre el CSV.
- **rm -f /dev/shm/*productos***  
  Limpia archivos de memoria compartida POSIX relacionados con el programa.
- **ipcrm -m <id> / ipcrm -s <id>**  
  Elimina segmentos de memoria y semáforos SysV huérfanos.

**Leyenda:**  
Estos comandos permiten evidenciar la creación y limpieza de recursos IPC, la concurrencia de procesos y el correcto funcionamiento del sistema.

------------------------------------------------------------
COMPILACIÓN
------------------------------------------------------------

Para compilar el programa principal:

    make

Esto generará el ejecutable `productos`.

------------------------------------------------------------
EJECUCIÓN BÁSICA
------------------------------------------------------------

Para ejecutar el programa con 4 generadores y 100 registros (valores por defecto):

    make run

Para especificar otra cantidad de generadores y registros (por ejemplo, 6 generadores y 250 registros):

    make run GENS=6 TOTAL=250

Esto generará el archivo `productos.csv`.

------------------------------------------------------------
MONITOREO DEL SISTEMA
------------------------------------------------------------

Para ejecutar el monitoreo completo (incluye validación automática):

    make monitorear

O con parámetros personalizados:

    make monitorear GENS=5 TOTAL=200

El monitoreo genera el log `monitoreo.log` y muestra información sobre procesos, recursos IPC y limpieza.

------------------------------------------------------------
VALIDACIÓN DEL ARCHIVO CSV
------------------------------------------------------------

Para validar el archivo generado:

    make validar

O con parámetros personalizados:

    make validar GENS=5 TOTAL=200

El script `validar.awk` revisa que los IDs sean correlativos, únicos y que la cantidad de generadores coincida.

------------------------------------------------------------
LIMPIEZA DE ARCHIVOS TEMPORALES
------------------------------------------------------------

Para limpiar todos los archivos generados (ejecutable, logs, CSV):

    make clean

------------------------------------------------------------
EJEMPLOS DE USO
------------------------------------------------------------

Compilar el programa:
    make

Ejecutar con 3 generadores y 50 registros:
    make run GENS=3 TOTAL=50

Monitorear con 8 generadores y 500 registros:
    make monitorear GENS=8 TOTAL=500

Validar el archivo generado:
    make validar GENS=8 TOTAL=500

Limpiar archivos temporales:
    make clean

------------------------------------------------------------
NOTAS
------------------------------------------------------------

- Si el programa o los scripts detectan parámetros inválidos, mostrarán ayuda y no ejecutarán acciones peligrosas.
- El monitoreo y la validación pueden ejecutarse independientemente del resto del sistema, siempre que existan los archivos necesarios.
- Todos los recursos IPC (memoria compartida, semáforos) se limpian automáticamente al finalizar.

------------------------------------------------------------
AUTORÍA Y LICENCIA
------------------------------------------------------------

Desarrollado para fines académicos.  
Puedes modificar y reutilizar el código citando la fuente.

# ===========================================================