/*
 * sensores.c
 *
 * Ejercicio 1 – Generador de datos con sensores simulados
 * Comunicación únicamente con memoria compartida y semáforos POSIX
 *
 * Uso:
 *   ./sensores <num_generadores> <total_registros>
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <errno.h>
#include <time.h>
#include <sys/wait.h>
#include <signal.h>

/* ============================================================
   DEFINES Y ESTRUCTURAS
   ============================================================ */
#define MAX_NOMBRE 32
#define MAX_TIPO 32
#define MAX_UNIDAD 8
#define MAX_TS 32
#define BLOCK_SIZE 10   // cantidad de IDs por bloque

// Registro que va en el archivo CSV
typedef struct {
    int id;
    char sensor[MAX_NOMBRE];
    char tipo[MAX_TIPO];
    double valor;
    char unidad[MAX_UNIDAD];
    char timestamp[MAX_TS];
} Registro;

// Estructura en memoria compartida
typedef struct {
    int next_id;       // próximo ID a asignar
    int total;         // total de registros a generar
    int written;       // cuántos ya escribió el coordinador
    Registro buffer;   // slot único para intercambio
} SharedData;

/* ============================================================
   VARIABLES GLOBALES
   ============================================================ */
static char shm_name[64];
static char sem_ids_name[64];
static char sem_empty_name[64];
static char sem_full_name[64];

// Semáforos
static sem_t *sem_ids   = NULL;
static sem_t *sem_empty = NULL;
static sem_t *sem_full  = NULL;

// Memoria compartida
static SharedData *shm  = NULL;
static int shm_fd = -1;

/* ============================================================
   FUNCIONES AUXILIARES
   ============================================================ */
void make_timestamp(char *buf, size_t bufsz) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(buf, bufsz, "%Y-%m-%d %H:%M:%S", &tm);
}

double gen_valor_por_tipo(const char *tipo) {
    if (strcmp(tipo, "Temperatura") == 0)
        return (rand() % 200) / 10.0 + 15.0; // 15-35
    else if (strcmp(tipo, "Humedad") == 0)
        return rand() % 101; // 0-100
    else if (strcmp(tipo, "CO2") == 0)
        return (rand() % 1001) + 300; // 300-1300
    else
        return rand() % 90 + 30; // Ruido 30-120
}

/* ============================================================
   FUNCIONES DEL GENERADOR (HIJO)
   ============================================================ */
void generador_loop(int idx) {
    srand(time(NULL) ^ getpid());

    const char *tipos[]   = {"Temperatura", "Humedad", "CO2", "Ruido"};
    const char *unidades[]= {"C", "%", "ppm", "dB"};
    const char *nombres[] = {"SensorA","SensorB","SensorC","SensorD"};

    const char *mi_tipo   = tipos[idx % 4];
    const char *mi_unidad = unidades[idx % 4];
    const char *mi_nombre = nombres[idx % 4];

    while (1) {
        sem_wait(sem_ids);
        int start = shm->next_id;
        int restante = shm->total - shm->next_id + 1;
        int count = (restante > 0) ? (restante < BLOCK_SIZE ? restante : BLOCK_SIZE) : 0;
        shm->next_id += count;
        sem_post(sem_ids);

        if (count == 0) break;

        for (int j = 0; j < count; j++) {
            int id = start + j;

            Registro reg;
            reg.id = id;
            snprintf(reg.sensor, MAX_NOMBRE, "%s", mi_nombre);
            snprintf(reg.tipo,   MAX_TIPO,    "%s", mi_tipo);
            reg.valor = gen_valor_por_tipo(mi_tipo);
            snprintf(reg.unidad, MAX_UNIDAD,  "%s", mi_unidad);
            make_timestamp(reg.timestamp, MAX_TS);

            sem_wait(sem_empty);
            shm->buffer = reg;
            sem_post(sem_full);

            // Usar nanosleep en lugar de usleep
            struct timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = (rand() % 300) * 1000 * 1000L;
            nanosleep(&ts, NULL);
        }
    }
    _exit(0);
}

/* ============================================================
   FUNCIONES DEL COORDINADOR (PADRE)
   ============================================================ */
void coordinador_loop(int total) {
    FILE *fp = fopen("salida.csv", "w");
    fprintf(fp, "ID,Sensor,Tipo,Valor,Unidad,Timestamp\n");

    while (1) {
        sem_wait(sem_full);
        Registro reg = shm->buffer;

        fprintf(fp, "%d,%s,%s,%.2f,%s,%s\n",
                reg.id, reg.sensor, reg.tipo,
                reg.valor, reg.unidad, reg.timestamp);
        fflush(fp);

        shm->written++;
        sem_post(sem_empty);

        if (shm->written >= total) break;
    }

    fclose(fp);
}

/* ============================================================
   MAIN
   ============================================================ */
int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <num_generadores> <total_registros>\n", argv[0]);
        return 1;
    }
    int num_generadores = atoi(argv[1]);
    int total = atoi(argv[2]);
    if (num_generadores <= 0 || total <= 0) {
        fprintf(stderr, "Parámetros inválidos.\n");
        return 1;
    }

    pid_t ppid = getpid();
    snprintf(shm_name, sizeof(shm_name), "/shm_%d", ppid);
    snprintf(sem_ids_name, sizeof(sem_ids_name), "/sem_ids_%d", ppid);
    snprintf(sem_empty_name, sizeof(sem_empty_name), "/sem_empty_%d", ppid);
    snprintf(sem_full_name, sizeof(sem_full_name), "/sem_full_%d", ppid);

    shm_fd = shm_open(shm_name, O_CREAT|O_RDWR, 0600);
    if (shm_fd == -1) { perror("shm_open"); exit(EXIT_FAILURE); }
    if (ftruncate(shm_fd, sizeof(SharedData)) == -1) { perror("ftruncate"); exit(EXIT_FAILURE); }

    shm = mmap(NULL, sizeof(SharedData), PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) { perror("mmap"); exit(EXIT_FAILURE); }

    shm->next_id = 1;
    shm->total = total;
    shm->written = 0;

    sem_ids   = sem_open(sem_ids_name,   O_CREAT, 0600, 1);
    sem_empty = sem_open(sem_empty_name, O_CREAT, 0600, 1);
    sem_full  = sem_open(sem_full_name,  O_CREAT, 0600, 0);

    for (int i = 0; i < num_generadores; i++) {
        if (fork() == 0) generador_loop(i);
    }

    coordinador_loop(total);

    for (int i = 0; i < num_generadores; i++) wait(NULL);

    sem_close(sem_ids);   sem_unlink(sem_ids_name);
    sem_close(sem_empty); sem_unlink(sem_empty_name);
    sem_close(sem_full);  sem_unlink(sem_full_name);

    munmap(shm, sizeof(SharedData));
    close(shm_fd); shm_unlink(shm_name);

    return 0;
}
