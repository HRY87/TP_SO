#ifndef TRANSACTION_H
#define TRANSACTION_H

// Función para iniciar una transacción
void begin_transaction(void);

// Función para confirmar una transacción
void commit_transaction(void);

// Función para cancelar una transacción
void rollback_transaction(void);

#endif // TRANSACTION_H