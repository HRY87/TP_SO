#ifndef DB_H
#define DB_H

extern char ARCHIVO_DB[512];

/* Operaciones que usan socket para responder al cliente */
void mostrar_registros(int socket_cliente);
void buscar_registro(int socket_cliente, const char *query);
void filtrar_generador(int socket_cliente, const char *generador);
/* DML */
void agregar_registro(const char *nuevo_registro);
void modificar_registro(const char *arg); /* formato: "ID;nueva_linea_completa" */
void eliminar_registro(const char *arg);
/* Commit de temp.csv sobre la BD (atómico si existe) */
int commit_temp(void);
#endif // DB_H