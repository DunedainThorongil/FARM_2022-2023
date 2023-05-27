#ifndef COLLECTOR_H_
#define COLLECTOR_H_

#include "util.h"

/**
 * @file  collector.h
 *
 * @brief Header file del Collector(processo figlio), contente la dichiarazione della struttura dati che riceverà il collector
 *        dai thread worker e il metodo dell'esecuzione del collector.
 *
 * @author  Alessandra De Lucrezia 565700
 *
 */

/**
 * @brief struttura dati del messaggio che il collector riceverà sulla connessione socket dai thread workers
 * @param conto valore numerico di tipo long ottenuto dal calcolo del file
 * @param pathfile nome del file di cui è stato eseguito il calcolo
 */
typedef struct msg_t
{
    long conto;
    char pathfile[MAXPATHLENGHT];
} msg_t;

/**
 * @brief Vengono effettuate la listen e la accept sulla socket ed utilizzando la select si selezionano i canali pronti
 *        Alla fine si stampa la lista dei file ricevuti, che sono stati inseriti alla ricezione in maniera ordinata.
 *        Alla ricezione del messaggio inviato dal thread_signal ci sono due comportamenti diversi:
 *              1) alla ricezione di tutti i segnali ad accezzione di SIGURS1 il collector stampa i file ricevuti fino a quel momento ed esce
 *              2) per il meassaggio ricevuto in seguito alla SystemCall SIGURS1 il collector stampa i file che hai ricevuto fino a quel momento e continua il suo lavoro
 *        In caso di fallimento ritorna -1 e libera la memoria occupata interrompendo la connessione socket
 *
 * @param fd_socket  identificatore socket
 *
 * @return (0) ok (-1) FAIL
 *
 */
int collector(int fd_sock);

#endif