#ifndef MASTER_WORKER_H_
#define MASTER_WORKER_H_

#include "threadpool.h"
#include "util.h"

/**
 * @file master_worker.h
 *
 * @author Alessandra De Lucrezia 565700
 *
 * @brief Headerfile del Master worker che si occupa di prendere i fila da linea di comando e da quelli all'interno della eventuale
 *  directory passata sulla CLI, ed assegnarli ai thread del threadpool.
 * 
 */

/**
 *
 * @brief Struct contenente le informazioni che il server si occupa di reperire dal file di configurazione
 *
 * @param argc
 * @param argv
 *
 * @param n_thread Specifica di quanti thread è composto il threadPool
 * @param queue_len Specifica la lunghezza della coda concorrente tra il thread Master ed i thread Worker (valore di default 8)
 * @param path_directory_name Specifica una directory in cui sono contenuti file binari ed eventualmente altre directory contenente file binari;
 * @param time Specifica un tempo in millisecondi che intercorre tra l’invio di due richieste successive ai thread Worker da parte del thread Master (valore di default 0)
 *
 * @param threadpool threadpool che sarà gestito dal threadMaster
 *
 */
typedef struct params_master_worker
{

    int argc;
    char **argv;

    long n_thread;
    long queue_len;
    char *path_directory_name;
    long time;

    threadpool_t *threadpool;

} params_master_worker_t;

/**
 *  @struct worker_thread_arg
 *  @brief Rappresenta argmenti da passare al memento di creazione del worker
 *
 *  @param file path del file da elaborare
 *  @param fd_sock identificatore della socket su cui inviare il risultato del calcolo
 */
typedef struct worker_thread_arg
{
    char file[255];
    int fd_skt;
} worker_thread_arg_t;

/**
 * @brief Funzione eseguita dal thread master.
 *        Viene analizzato tutto argv per trovare i file.dat da mandare ad i thread del threadpool, e
 *        la directory(in cui sono presenti altri file.dat e/o altre directory)
 * @param param struttura dati contenente tutte le informazioni che che serviranno al threadMaster per lavorare
 */
void *master_worker(void *param);

/**
 * @brief funzione eseguita dai thread del threadpool che apriranno i file binari, ne calcoleranno il risultato e lo manderanno al collector
 * @param param struttura dati che conterrà il path assoluto del file e fd_skt
 */
void worker(void *param);

#endif
