# Mini Database Server in C

Este proyecto implementa un sistema cliente-servidor para manejar una mini base de datos utilizando un archivo CSV (`productos.csv`). El servidor permite a múltiples clientes realizar consultas y modificaciones sobre la base de datos de manera concurrente, gestionando transacciones para asegurar la integridad de los datos.

## Estructura del Proyecto

```
mini-db-server-c
├── src
│   ├── servidor.c         # Implementación del servidor que maneja conexiones y consultas.
│   ├── cliente.c          # Implementación del cliente que se conecta al servidor.
│   ├── db.c               # Funciones para manipulación de la base de datos.
│   ├── transaction.c       # Lógica de manejo de transacciones.
│   └── utils.c            # Funciones utilitarias para el servidor y cliente.
├── include
│   ├── db.h               # Declaraciones de funciones para la base de datos.
│   ├── transaction.h      # Declaraciones de funciones para la gestión de transacciones.
│   └── utils.h            # Declaraciones de funciones utilitarias.
├── data
│   └── productos.csv      # Archivo CSV que contiene los registros de productos.
├── config
│   └── server.conf        # Archivo de configuración para el servidor.
├── scripts
│   └── run_server.sh      # Script para compilar y ejecutar el servidor.
├── Makefile               # Instrucciones para compilar el proyecto.
├── .gitignore             # Archivos y directorios a ignorar por Git.
└── README.md              # Documentación del proyecto.
```

## Requisitos

- El servidor debe aceptar hasta N clientes concurrentes y mantener hasta M clientes en espera.
- Los clientes pueden realizar consultas (buscar, filtrar, mostrar) y modificaciones (agregar, modificar, eliminar) sobre la base de datos.
- Las transacciones permiten a los clientes realizar múltiples modificaciones antes de confirmar o cancelar.
- El servidor maneja cierres inesperados de clientes y asegura que no haya conflictos durante las transacciones.

## Instrucciones de Uso

1. **Configuración del Servidor**: Edita el archivo `config/server.conf` para especificar la dirección IP y el puerto de escucha.
2. **Compilación**: Ejecuta `make` en la raíz del proyecto para compilar el servidor y el cliente.
3. **Ejecución**: Usa el script `scripts/run_server.sh` para iniciar el servidor.
4. **Conexión del Cliente**: Ejecuta el cliente para conectarte al servidor y comenzar a realizar consultas y modificaciones.

## Contribuciones

Las contribuciones son bienvenidas. Por favor, abre un issue o un pull request para discutir cambios o mejoras.

## Licencia

Este proyecto está bajo la Licencia MIT.