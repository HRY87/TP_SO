#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "db.h"
#include "utils.h"

char ARCHIVO_DB[512] = "data/productos.csv";

// Muestra todos los registros de la base de datos al socket
void mostrar_registros(int socket_cliente) {
    FILE *archivo = fopen(ARCHIVO_DB, "r");
    if (!archivo) {
        enviar(socket_cliente, "Error al abrir el archivo de base de datos.\n");
        return;
    }
    char linea[512];
    while (fgets(linea, sizeof(linea), archivo)) {
        enviar(socket_cliente, linea);
    }
    fclose(archivo);
}

// Busca por substring en todo el registro (query simple)
void buscar_registro(int socket_cliente, const char *query) {
    if (!query || strlen(query) == 0) {
        enviar(socket_cliente, "BUSCAR requiere un criterio.\n");
        return;
    }
    // quitar posible espacio inicial
    while (*query == ' ') query++;
    FILE *archivo = fopen(ARCHIVO_DB, "r");
    if (!archivo) {
        enviar(socket_cliente, "Error al abrir el archivo de base de datos.\n");
        return;
    }
    char linea[512];
    int encontrado = 0;
    while (fgets(linea, sizeof(linea), archivo)) {
        if (strstr(linea, query) != NULL) {
            enviar(socket_cliente, linea);
            encontrado = 1;
        }
    }
    if (!encontrado) enviar(socket_cliente, "No se encontraron registros.\n");
    fclose(archivo);
}

// Agrega un nuevo registro (línea completa ya formateada)
void agregar_registro(const char *nuevo_registro) {
    FILE *archivo = fopen(ARCHIVO_DB, "a+");
    if (!archivo) {
        perror("Error al abrir el archivo de base de datos");
        return;
    }
    // asegurar que el archivo termina en newline antes de añadir
    fseek(archivo, 0, SEEK_END);
    long pos = ftell(archivo);
    if (pos > 0) {
        if (fseek(archivo, pos - 1, SEEK_SET) == 0) {
            int last = fgetc(archivo);
            if (last != '\n') fputc('\n', archivo);
        }
    }
    fprintf(archivo, "%s\n", nuevo_registro);
    fclose(archivo);
}

// Modifica registro: cadena esperada: "<ID>;<nueva_linea_completa>"
void modificar_registro(const char *arg) {
    if (!arg) return;
    // formato: id;nuevo_registro
    char copia[1024];
    strncpy(copia, arg, sizeof(copia)-1);
    copia[sizeof(copia)-1] = '\0';
    char *id_str = strtok(copia, ";");
    char *nuevo = strtok(NULL, "");
    if (!id_str || !nuevo) {
        printf("MODIFICAR: formato inválido. Uso: MODIFICAR <ID>;<nueva_linea_completa>\n");
        return;
    }
    int id = atoi(id_str);
    FILE *archivo = fopen(ARCHIVO_DB, "r");
    FILE *temp = fopen("temp.csv", "w");
    if (!archivo || !temp) {
        perror("Error al abrir archivos para modificar");
        if (archivo) fclose(archivo);
        if (temp) fclose(temp);
        return;
    }
    char linea[1024];
    int encontrado = 0;
    while (fgets(linea, sizeof(linea), archivo)) {
        char linea_c[1024];
        strncpy(linea_c, linea, sizeof(linea_c)-1);
        linea_c[sizeof(linea_c)-1] = '\0';
        char *tok = strtok(linea_c, ",");
        int id_arch = atoi(tok);
        if (id_arch == id) {
            fprintf(temp, "%s\n", nuevo);
            encontrado = 1;
        } else {
            fputs(linea, temp);
        }
    }
    fclose(archivo);
    fclose(temp);
    // reemplazo atómico
    remove(ARCHIVO_DB);
    rename("temp.csv", ARCHIVO_DB);
    if (encontrado) printf("Registro %d modificado.\n", id);
    else printf("Registro %d no encontrado para modificar.\n", id);
}

// Elimina registro por ID (arg = "<ID>")
void eliminar_registro(const char *arg) {
    if (!arg) return;
    int id = atoi(arg);
    FILE *archivo = fopen(ARCHIVO_DB, "r");
    FILE *temp = fopen("temp.csv", "w");
    if (!archivo || !temp) {
        perror("Error al abrir archivos para eliminar");
        if (archivo) fclose(archivo);
        if (temp) fclose(temp);
        return;
    }
    char linea[1024];
    int encontrado = 0;
    while (fgets(linea, sizeof(linea), archivo)) {
        char copia[1024];
        strncpy(copia, linea, sizeof(copia)-1);
        copia[sizeof(copia)-1] = '\0';
        char *tok = strtok(copia, ",");
        int id_arch = atoi(tok);
        if (id_arch != id) fputs(linea, temp);
        else encontrado = 1;
    }
    fclose(archivo);
    fclose(temp);
    remove(ARCHIVO_DB);
    rename("temp.csv", ARCHIVO_DB);
    if (encontrado) printf("Registro %d eliminado.\n", id);
    else printf("Registro %d no encontrado para eliminar.\n", id);
}

// Filtra registros por número de generador (ej. "1")
void filtrar_generador(int socket_cliente, const char *generador) {
    if (!generador) {
        enviar(socket_cliente, "FILTRO requiere un número de generador.\n");
        return;
    }
    // quitar espacios iniciales
    while (*generador == ' ') generador++;
    if (*generador == '\0') {
        enviar(socket_cliente, "FILTRO requiere un número de generador.\n");
        return;
    }
    int gen = atoi(generador);
    if (gen <= 0) {
        enviar(socket_cliente, "FILTRO: generador inválido.\n");
        return;
    }

    FILE *archivo = fopen(ARCHIVO_DB, "r");
    if (!archivo) {
        enviar(socket_cliente, "Error al abrir el archivo de base de datos.\n");
        return;
    }
    char linea[2048];
    int encontrado = 0;
    while (fgets(linea, sizeof(linea), archivo)) {
        // copiar porque strtok modifica
        char copia[2048];
        strncpy(copia, linea, sizeof(copia)-1);
        copia[sizeof(copia)-1] = '\0';
        // obtener último token (generador)
        char *tok = strtok(copia, ",");
        char *last = tok;
        while (tok) {
            last = tok;
            tok = strtok(NULL, ",");
        }
        if (last) {
            // quitar newline y espacios
            char *p = last;
            while (*p && *p != '\n' && *p != '\r') p++;
            *p = '\0';
            if (atoi(last) == gen) {
                enviar(socket_cliente, linea);
                encontrado = 1;
            }
        }
    }
    if (!encontrado) enviar(socket_cliente, "No se encontraron registros para ese generador.\n");
    fclose(archivo);
}

// Aplica archivo temporal (temp.csv) sobre la base de datos (atómico)
int commit_temp() {
    // intenta eliminar original y renombrar temp
    if (access("temp.csv", F_OK) != 0) {
        // no hay temp
        return -1;
    }
    if (remove(ARCHIVO_DB) != 0) {
        // intentar continuar aun si falla remove
        // opcional: log_msg("warning: remove db failed: %d", errno);
    }
    if (rename("temp.csv", ARCHIVO_DB) != 0) {
        perror("rename temp -> db falló");
        return -1;
    }
    return 0;
}