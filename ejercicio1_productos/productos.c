/*
 * productos.c
 *
 * Generación de productos con procesos en paralelo.
 * Memoria compartida y semáforos POSIX.
 *
 * Uso:
 *   ./productos <num_generadores> <total_productos>
 *
 * Ejemplo:
 *   ./productos 2 50
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

#define MAX_DESC 64
#define MAX_FECHA 16
#define MAX_HORA 16
#define BLOQUE_IDS 10

typedef struct {
    int id;
    char descripcion[MAX_DESC];
    int cantidad;
    char fecha[MAX_FECHA];
    char hora[MAX_HORA];
} Producto;

typedef struct {
    int siguiente_id;     // próximo ID a asignar (global)
    int total;            // total de registros a generar
    int escritos;         // cuantos ya fueron escritos
    int producers_alive;  // cuántos generadores todavía existen
    Producto buffer;      // slot único para intercambio
} MemCompartida;

/* Nombres de recursos dependientes del pid padre */
static char nombre_shm[64];
static char sem_ids_nombre[64];
static char sem_vacio_nombre[64];
static char sem_lleno_nombre[64];

static sem_t *sem_ids = NULL;
static sem_t *sem_vacio = NULL;
static sem_t *sem_lleno = NULL;
static MemCompartida *mem = NULL;
static int shm_fd = -1;

static pid_t *child_pids = NULL;
static int num_generadores_g = 0;

/* Prototipos */
void limpiar_recursos(void);
void handle_signal(int sig);

/* ===================== Limpieza y señales ===================== */

void limpiar_recursos(void) {
    /* Solo el proceso que tenga los descriptores/handles debe unlinkear.
       Sem_close/sem_unlink y shm_unlink son tolerantes si no existen. */
    if (sem_ids) { sem_close(sem_ids); sem_unlink(sem_ids_nombre); sem_ids = NULL; }
    if (sem_vacio) { sem_close(sem_vacio); sem_unlink(sem_vacio_nombre); sem_vacio = NULL; }
    if (sem_lleno) { sem_close(sem_lleno); sem_unlink(sem_lleno_nombre); sem_lleno = NULL; }

    if (mem) { munmap(mem, sizeof(MemCompartida)); mem = NULL; }
    if (shm_fd != -1) { close(shm_fd); shm_fd = -1; shm_unlink(nombre_shm); }
}

void handle_signal(int sig) {
    (void)sig;
    fprintf(stderr, "[PADRE] señal recibida, limpiando recursos y terminando\n");
    if (child_pids) {
        for (int i = 0; i < num_generadores_g; ++i) {
            if (child_pids[i] > 0) kill(child_pids[i], SIGTERM);
        }
    }
    limpiar_recursos();
    _exit(EXIT_FAILURE);
}

/* ===================== Utilidades ===================== */

void make_fecha_hora(char *fecha, size_t fsz, char *hora, size_t hsz) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(fecha, fsz, "%Y-%m-%d", &tm);
    strftime(hora, hsz, "%H:%M:%S", &tm);
}

/* ===================== Generador (hijo) ===================== */

void generador_loop(int idx) {
    /* Restaurar señales a default en el hijo (para que no ejecuten manejadores del padre).
       También evitar que el hijo ejecute el atexit del padre. */
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);

    srand((unsigned int)time(NULL) ^ (unsigned int)getpid());

    int local_seq = 0; /* contador local por generador que no se reinicia entre bloques */

    while (1) {
        /* Pedir bloque de IDs de forma atómica */
        while (sem_wait(sem_ids) == -1) {
            if (errno == EINTR) continue;
            perror("[GEN] sem_wait(sem_ids)");
            _exit(EXIT_FAILURE);
        }
        int inicio = mem->siguiente_id;
        int restante = mem->total - mem->siguiente_id + 1;
        int cantidad = (restante > 0) ? (restante < BLOQUE_IDS ? restante : BLOQUE_IDS) : 0;
        if (cantidad > 0) mem->siguiente_id += cantidad;
        if (sem_post(sem_ids) == -1) {
            perror("[GEN] sem_post(sem_ids)");
            _exit(EXIT_FAILURE);
        }

        if (cantidad == 0) break;

        for (int j = 0; j < cantidad; ++j) {
            Producto p;
            p.id = inicio + j;
            local_seq++;
            snprintf(p.descripcion, sizeof(p.descripcion), "G%d_%03d", idx, local_seq);
            p.cantidad = (rand() % 50) + 1;
            make_fecha_hora(p.fecha, sizeof(p.fecha), p.hora, sizeof(p.hora));

            /* esperar espacio libre en buffer (slot único) */
            while (sem_wait(sem_vacio) == -1) {
                if (errno == EINTR) continue;
                perror("[GEN] sem_wait(sem_vacio)");
                _exit(EXIT_FAILURE);
            }

            /* escribir registro en memoria compartida (una sola celda) */
            mem->buffer = p;

            if (sem_post(sem_lleno) == -1) {
                perror("[GEN] sem_post(sem_lleno)");
                _exit(EXIT_FAILURE);
            }

            /* pequeña pausa aleatoria para simular trabajo/concurrencia */
            struct timespec ts = {0, (rand() % 300) * 1000000L};
            nanosleep(&ts, NULL);
        }
    }

    /* decrementamos contador de productores vivos (protégelo con sem_ids) */
    while (sem_wait(sem_ids) == -1) {
        if (errno == EINTR) continue;
        break;
    }
    if (mem->producers_alive > 0) mem->producers_alive--;
    sem_post(sem_ids);

    _exit(EXIT_SUCCESS);
}

/* ===================== Coordinador (padre) ===================== */

void coordinador_loop(int total) {
    FILE *fp = fopen("productos.csv", "w");
    if (!fp) {
        perror("[COORD] fopen");
        return;
    }
    fprintf(fp, "ID,Descripcion,Cantidad,Fecha,Hora\n");
    fflush(fp);

    while (1) {
        /* usamos sem_timedwait para evitar bloqueo indefinido */
        struct timespec now, timeout;
        clock_gettime(CLOCK_REALTIME, &now);
        timeout = now; timeout.tv_sec += 1; /* 1 segundo timeout */

        int s = sem_timedwait(sem_lleno, &timeout);
        if (s == -1) {
            if (errno == ETIMEDOUT) {
                /* revisar si ya no quedan productores y no quedan registros pendientes */
                if (mem->producers_alive == 0) {
                    int expected_written = (mem->total < (mem->siguiente_id - 1)) ? mem->total : (mem->siguiente_id - 1);
                    if (mem->escritos >= expected_written) break;
                }
                continue;
            } else if (errno == EINTR) {
                continue;
            } else {
                perror("[COORD] sem_timedwait(sem_lleno)");
                break;
            }
        }

        /* leemos buffer (un registro) */
        Producto p = mem->buffer;
        fprintf(fp, "%d,%s,%d,%s,%s\n",
                p.id, p.descripcion, p.cantidad, p.fecha, p.hora);
        fflush(fp);

        /* actualizar contadores - proteger con sem_ids */
        while (sem_wait(sem_ids) == -1) {
            if (errno == EINTR) continue;
            perror("[COORD] sem_wait(sem_ids)");
            break;
        }
        mem->escritos++;
        sem_post(sem_ids);

        if (sem_post(sem_vacio) == -1) {
            perror("[COORD] sem_post(sem_vacio)");
            break;
        }

        if (mem->escritos >= total) break;
    }

    fclose(fp);
}

/* ===================== MAIN ===================== */

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <num_generadores> <total_productos>\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s 2 50\n", argv[0]);
        return EXIT_FAILURE;
    }

    int num_generadores = atoi(argv[1]);
    int total = atoi(argv[2]);
    if (num_generadores <= 0 || total <= 0) {
        fprintf(stderr, "Parámetros inválidos: deben ser enteros positivos.\n");
        return EXIT_FAILURE;
    }

    num_generadores_g = num_generadores;

    /* Nombres dependientes del pid padre para evitar colisiones */
    pid_t ppid = getpid();
    snprintf(nombre_shm, sizeof(nombre_shm), "/shm_prod_%d", ppid);
    snprintf(sem_ids_nombre, sizeof(sem_ids_nombre), "/sem_ids_%d", ppid);
    snprintf(sem_vacio_nombre, sizeof(sem_vacio_nombre), "/sem_vacio_%d", ppid);
    snprintf(sem_lleno_nombre, sizeof(sem_lleno_nombre), "/sem_lleno_%d", ppid);

    /* crear shm (solo padre) */
    shm_fd = shm_open(nombre_shm, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (shm_fd == -1) {
        if (errno == EEXIST) {
            fprintf(stderr, "Error: recurso shm ya existente %s\n", nombre_shm);
        }
        perror("shm_open");
        return EXIT_FAILURE;
    }
    if (ftruncate(shm_fd, sizeof(MemCompartida)) == -1) {
        perror("ftruncate");
        shm_unlink(nombre_shm);
        return EXIT_FAILURE;
    }

    mem = mmap(NULL, sizeof(MemCompartida), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (mem == MAP_FAILED) {
        perror("mmap");
        shm_unlink(nombre_shm);
        return EXIT_FAILURE;
    }

    /* inicializar shared */
    mem->siguiente_id = 1;
    mem->total = total;
    mem->escritos = 0;
    mem->producers_alive = num_generadores;

    /* crear semáforos (solo padre) */
    sem_ids = sem_open(sem_ids_nombre, O_CREAT | O_EXCL, 0600, 1);
    sem_vacio = sem_open(sem_vacio_nombre, O_CREAT | O_EXCL, 0600, 1);
    sem_lleno = sem_open(sem_lleno_nombre, O_CREAT | O_EXCL, 0600, 0);
    if (sem_ids == SEM_FAILED || sem_vacio == SEM_FAILED || sem_lleno == SEM_FAILED) {
        perror("sem_open");
        limpiar_recursos();
        return EXIT_FAILURE;
    }

    /* instalar manejadores de señal en el padre (no en hijos) */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* forkar generadores */
    child_pids = calloc(num_generadores, sizeof(pid_t));
    if (!child_pids) {
        perror("calloc");
        limpiar_recursos();
        return EXIT_FAILURE;
    }

    for (int i = 0; i < num_generadores; ++i) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            continue;
        }
        if (pid == 0) {
            /* hijo: ejecutar generador (no debe ejecutar atexit del padre ni handlers del padre) */
            generador_loop(i + 1);
            /* nunca retorna */
        } else {
            child_pids[i] = pid;
        }
    }

    /* Registrar limpieza al salir - SOLO en el padre */
    atexit(limpiar_recursos);

    /* coordinador (padre) */
    coordinador_loop(total);

    /* esperar hijos */
    for (int i = 0; i < num_generadores; ++i) {
        if (child_pids[i] > 0) waitpid(child_pids[i], NULL, 0);
    }

    /* Imprimir resumen ANTES de limpiar recursos */
    printf("Generación completada: %d registros escritos.\n", mem->escritos);

    /* limpieza final (atexit también lo hará, pero cerramos explícitamente) */
    limpiar_recursos();
    free(child_pids);

    return EXIT_SUCCESS;
}
