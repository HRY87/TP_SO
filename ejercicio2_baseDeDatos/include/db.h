#ifndef DB_H
#define DB_H

extern char ARCHIVO_DB[512];

/* Operaciones que usan socket para responder al cliente */
void mostrar_registros(int socket_cliente);
void buscar_registro(int socket_cliente, const char *query);
void filtrar_generador(int socket_cliente, const char *generador);
/* DML */
int agregar_registro(const char *nuevo_registro);
int modificar_registro(int socket_cliente, const char *arg); /* formato: "ID;nueva_linea_completa" */
int eliminar_registro(const char *arg);
int rollback_transaccion(void); /* Elimina temp.csv si existe */
/* Commit de temp.csv sobre la BD (atómico si existe) */
int commit_temp(void);
#endif // DB_H