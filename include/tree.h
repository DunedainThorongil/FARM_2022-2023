#if !defined(TREE_H)
#define TREE_H

#include "util.h"
#include "collector.h"
/*
 * @file tree.c
 *
 * @brief Header file contenente la dichiarazione della struttura di un albero binario con inserimento ordinato
 *
 * @author Alessandra De Lucrezia 565700
 *
 */
typedef struct Nodo
{
    msg_t chiave;
    struct Nodo *sinistra;
    struct Nodo *destra;
} Nodo;

/**
 * @brief metodo che crea un nodo dell'albero
 * @param chiave valore del nodo da creare
 */
Nodo *creaNodo(msg_t chiave);

/**
 * @brief metodo che inserisce un nodo dell'albero in maniera ordinata
 * @param radice radice dell'albero a cui verrà aggiunto un nodo
 * @param chiave valore del nuovo nodo
 */
Nodo *inserisci(Nodo *radice, msg_t chiave);

/**
 * @brief metodo che stampa l'albero
 * @param radice radice dell'albero da stampare
 */
void stampaInOrdine(Nodo *radice);

/**
 * @brief metodo che dealloca lo spazio usato dall'labero
 * @param radice radice dell'albero da deallocare
 */
void liberaAlbero(Nodo *radice);

/**
 * @brief Metodo che prende il path assoluto e ritorna solo il path dalla directory corrente al nome del file.
 * 
 * @param msg struttura dati in cui verrà sostituito il path iniziale memorizzato in msg.pathfile con il nuovo valore calcolato
 * @param currDir path della directory corrente
 * @return (0) OK (-1) FAIL
 */
int short_path(msg_t *msg, char* currDir);

#endif