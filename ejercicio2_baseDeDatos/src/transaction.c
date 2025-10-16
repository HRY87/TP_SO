#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "transaction.h"
#include "db.h"

extern int transaccion_activa;
extern pthread_mutex_t mutex_transaccion;

// Inicia una transacción
void begin_transaction() {
    pthread_mutex_lock(&mutex_transaccion);
    if (transaccion_activa) {
        printf("Error: ya existe una transacción activa.\n");
    } else {
        transaccion_activa = 1;
        printf("Transacción iniciada.\n");
    }
    pthread_mutex_unlock(&mutex_transaccion);
}

// Confirma la transacción
void commit_transaction() {
    pthread_mutex_lock(&mutex_transaccion);
    if (!transaccion_activa) {
        printf("Error: no hay transacción activa para confirmar.\n");
    } else {
        transaccion_activa = 0;
        printf("Transacción confirmada.\n");
    }
    pthread_mutex_unlock(&mutex_transaccion);
}

// Cancela la transacción
void rollback_transaction() {
    pthread_mutex_lock(&mutex_transaccion);
    if (!transaccion_activa) {
        printf("Error: no hay transacción activa para cancelar.\n");
    } else {
        transaccion_activa = 0;
        printf("Transacción cancelada.\n");
    }
    pthread_mutex_unlock(&mutex_transaccion);
}