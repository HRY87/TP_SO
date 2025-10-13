/* 
 *  productos.c
 *
 * Generación de productos con procesos en paralelo.
 * Memoria compartida y semáforos POSIX.
 *
 * Uso:
 *   ./productos <num_generadores> <total_productos>
 *
 * Ejemplo:
 *   ./productos 4 100
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

/* Tamaño del bloque que pide cada generador (10 según tu requerimiento) */
#define BLOQUE_IDS 10
#define MAX_DESC 64
#define MAX_FECHA 16
#define MAX_HORA 16

typedef struct {
    int id;
    char descripcion[MAX_DESC];
    int cantidad;
    char fecha[MAX_FECHA];
    char hora[MAX_HORA];
} Producto;

/* Buffer compartido en shm: contiene producto + quién lo generó */
typedef struct {
    Producto p;
    int generador; /* índice del generador (1..N) que produjo el registro */
} BufferEntry;

/* Estructura de memoria compartida */
typedef struct {
    int siguiente_id;     /* próximo ID global disponible: 1..total */
    int total;            /* total de registros a generar */
    int escritos;         /* cuantos ya fueron leídos/escritos por el coordinador */
    int producers_alive;  /* cuántos generadores siguen vivos */
    BufferEntry buffer;   /* slot único para intercambio (product + generador) */
} MemCompartida;

/* Nombres dependientes del pid padre */
static char nombre_shm[64];
static char sem_ids_nombre[64];
static char sem_vacio_nombre[64];
static char sem_lleno_nombre[64];

static sem_t *sem_ids = NULL;   /* mutex para acceder a campos como siguiente_id, producers_alive, escritos */
static sem_t *sem_vacio = NULL; /* indica que el buffer está vacío (puede escribir un productor) */
static sem_t *sem_lleno = NULL; /* indica que el buffer está lleno (hay registro para leer) */

static MemCompartida *mem = NULL;
static int shm_fd = -1;

static pid_t *child_pids = NULL;
static int num_generadores_g = 0;

/* Flag para SIGCHLD: handler sólo establece la flag (async-signal-safe) */
static volatile sig_atomic_t sigchld_flag = 0;

/* Prototipos */
void limpiar_recursos(void);
void handle_signal(int sig);
void sigchld_handler(int sig);
void reap_children(void);
void generador_loop(int idx);
void coordinador_loop(int total);

/* ===================== Limpieza y señales ===================== */

void limpiar_recursos(void) {
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

/* Handler SIGCHLD: sólo marca la flag. No hacer work pesado aquí. */
void sigchld_handler(int sig) {
    (void)sig;
    sigchld_flag = 1;
}

/* Reap children (llamar desde contexto seguro, p.ej. el bucle principal).
   Hace waitpid(..., WNOHANG) y decrementa producers_alive protegido por sem_ids. */
void reap_children(void) {
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (sem_ids != NULL && mem != NULL) {
            if (sem_wait(sem_ids) == 0) {
                if (mem->producers_alive > 0) mem->producers_alive--;
                if (sem_post(sem_ids) == -1) {
                    perror("[PADRE] sem_post(sem_ids) en reap_children");
                }
            } else {
                if (errno != EINTR) {
                    perror("[PADRE] sem_wait(sem_ids) en reap_children");
                }
            }
        }
        fprintf(stderr, "[PADRE] reap_children: hijo %d finalizó. producers_alive=%d\n",
                pid, mem ? mem->producers_alive : -1);
    }
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
    /* Restaurar señales a default en el hijo */
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);

    srand((unsigned int)time(NULL) ^ (unsigned int)getpid());

    int local_seq = 0; /* secuencia local por generador */

    while (1) {
        /* Pedir bloque de IDs de forma atómica (protección con sem_ids) */
        while (1) {
            if (sem_wait(sem_ids) == 0) break;
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
            while (1) {
                if (sem_wait(sem_vacio) == 0) break;
                if (errno == EINTR) continue;
                perror("[GEN] sem_wait(sem_vacio)");
                _exit(EXIT_FAILURE);
            }

            /* escribir registro + id del generador en memoria compartida */
            mem->buffer.p = p;
            mem->buffer.generador = idx;

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
    while (1) {
        if (sem_wait(sem_ids) == 0) break;
        if (errno == EINTR) continue;
        perror("[GEN] sem_wait(sem_ids) al decrementar producers_alive");
        break;
    }
    if (mem->producers_alive > 0) mem->producers_alive--;
    if (sem_post(sem_ids) == -1) {
        perror("[GEN] sem_post(sem_ids) al liberar lock");
    }

    /* Cerrar semáforos en hijo (no unlink) */
    if (sem_ids) sem_close(sem_ids);
    if (sem_vacio) sem_close(sem_vacio);
    if (sem_lleno) sem_close(sem_lleno);

    _exit(EXIT_SUCCESS);
}

/* ===================== Coordinador (padre) ===================== */

/* Estructura para almacenar en memoria los productos y luego escribir ordenados */
typedef struct {
    Producto p;
    int generador;
    int present;
} StoredProd;

void coordinador_loop(int total) {
    /* Reservar arreglo temporal para guardar TOTAL registros y luego escribir ordenado */
    StoredProd *arr = calloc((size_t)total, sizeof(StoredProd));
    if (!arr) {
        perror("[COORD] calloc arr");
        return;
    }

    /* También contadores por generador para resumen */
    int *contador_por_gen = calloc((size_t)num_generadores_g + 1, sizeof(int));
    if (!contador_por_gen) {
        perror("[COORD] calloc contador_por_gen");
        free(arr);
        return;
    }

    /* loop de lectura de buffer */
    while (1) {
        /* manejar SIGCHLD si llegaron */
        if (sigchld_flag) {
            sigchld_flag = 0;
            reap_children();
        }

        /* usar sem_timedwait para no bloquear indefinidamente y poder
           comprobar productores vivos y finalizar correctamente */
        struct timespec now, timeout;
        clock_gettime(CLOCK_REALTIME, &now);
        timeout = now; timeout.tv_sec += 1; /* 1 segundo timeout */

        int s = sem_timedwait(sem_lleno, &timeout);
        if (s == -1) {
            if (errno == ETIMEDOUT) {
                int producers = mem ? mem->producers_alive : 0;
                /* si ya no hay productores y ya leímos TODOS los IDs asignados, terminar */
                if (producers == 0) {
                    /* expected_written: lo que se llegó a asignar (siguiente_id - 1), máximo total */
                    int assigned = (mem->siguiente_id - 1);
                    if (assigned > mem->total) assigned = mem->total;
                    if (mem->escritos >= assigned) break;
                }
                continue;
            } else if (errno == EINTR) {
                continue;
            } else {
                perror("[COORD] sem_timedwait(sem_lleno)");
                break;
            }
        }

        /* leemos buffer (producto + generador) */
        BufferEntry be = mem->buffer;
        Producto p = be.p;
        int gen = be.generador;

        /* almacenar en arreglo por ID (IDs comienzan en 1) */
        if (p.id >= 1 && p.id <= total) {
            int idx = p.id - 1;
            arr[idx].p = p;
            arr[idx].generador = gen;
            arr[idx].present = 1;
            if (gen >=1 && gen <= num_generadores_g) contador_por_gen[gen]++;
        } else {
            /* ID fuera de rango: lo registramos en stderr pero igualmente contamos */
            fprintf(stderr, "[COORD] recibido ID fuera de rango: %d (total=%d)\n", p.id, total);
        }

        /* actualizar contadores - proteger con sem_ids */
        while (1) {
            if (sem_wait(sem_ids) == 0) break;
            if (errno == EINTR) continue;
            perror("[COORD] sem_wait(sem_ids)");
            break;
        }
        mem->escritos++;
        if (sem_post(sem_ids) == -1) {
            perror("[COORD] sem_post(sem_ids)");
        }

        /* liberar slot (vacio) para que productores sigan */
        if (sem_post(sem_vacio) == -1) {
            perror("[COORD] sem_post(sem_vacio)");
            break;
        }

        /* condición de salida: cuando leímos al menos "total" registros válidos */
        if (mem->escritos >= total) break;
    }

    /* Escribir CSV ordenado por ID */
    FILE *fp = fopen("productos.csv", "w");
    if (!fp) {
        perror("[COORD] fopen productos.csv");
    } else {
        fprintf(fp, "ID,Descripcion,Cantidad,Fecha,Hora,Generador\n");
        for (int i = 0; i < total; ++i) {
            if (arr[i].present) {
                Producto *pp = &arr[i].p;
                fprintf(fp, "%d,%s,%d,%s,%s,%d\n",
                        pp->id, pp->descripcion, pp->cantidad, pp->fecha, pp->hora,
                        arr[i].generador);
            } else {
                /* Si faltan IDs (no present), escribir línea con aviso o ignorar.
                   Aquí escribimos una línea comentada para facilitar debugging. */
                fprintf(fp, "#MISSING,%d\n", i+1);
            }
        }
        fclose(fp);
    }

    /* Imprimir resumen en stdout */
    int total_escritos = 0;
    for (int g = 1; g <= num_generadores_g; ++g) total_escritos += contador_por_gen[g];
    printf("Generación completada: %d registros almacenados (padre contó %d escrituras).\n",
           total_escritos, mem ? mem->escritos : -1);
    printf("Resumen por generador:\n");
    for (int g = 1; g <= num_generadores_g; ++g) {
        printf("  Generador %d: %d registros\n", g, contador_por_gen[g]);
    }

    free(arr);
    free(contador_por_gen);
}

/* ===================== MAIN ===================== */

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <num_generadores> <total_productos>\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s 5 100\n", argv[0]);
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
    /* buffer no necesita inicializarse estrictamente */

    /* crear semáforos (solo padre) */
    sem_ids = sem_open(sem_ids_nombre, O_CREAT | O_EXCL, 0600, 1);
    sem_vacio = sem_open(sem_vacio_nombre, O_CREAT | O_EXCL, 0600, 1); /* buffer initially empty */
    sem_lleno = sem_open(sem_lleno_nombre, O_CREAT | O_EXCL, 0600, 0); /* nothing to read initially */
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

    /* instalar handler SIGCHLD (solo marca flag) */
    struct sigaction sa_chld;
    memset(&sa_chld, 0, sizeof(sa_chld));
    sa_chld.sa_handler = sigchld_handler;
    sa_chld.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa_chld, NULL);

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

    /* Imprimir resumen final (antes de limpiar) */
    printf("Proceso padre finaliza. Registros leídos por coordinador: %d\n", mem ? mem->escritos : -1);

    /* limpieza final */
    limpiar_recursos();
    free(child_pids);

    return EXIT_SUCCESS;
}
