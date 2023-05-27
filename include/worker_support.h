#ifndef WORKER_SUPPORT_H
#define WORKER_SUPPORT_H

/**
 * @file worker_support.h
 *
 * @author Alessandra De Lucrezia 565700
 *
 * @brief Header file contenente la dichiarazione dei metodi di supporto per il master Worker e per i  thread workers
 *
 */
#include "master_worker.h"
#include "util.h"
#include "collector.h"

#define DEFAULT_WORKERS 4
#define DEFAULT_QUEUE 8
#define DEFAULT_TIME 0

/**
 * @brief Metodo che controlla se un file ha l'estensione .dat
 * Vengono analizzati gli ultimi 4 caratteri del file_name, se sono == ".dat" allora il file ha un'esensione binaria
 *
 * @param file_name nome del file
 *
 * @return (0) OK è un file binario (-1) FAIL
 */
int file_dat(char *file_name);

/**
 * @brief metodo che controlla se dir è formata solo da '.'
 * @param dir stringa da controllare
 * @return 0 (OK) -1 (FAIL)
 */
int dots_only(const char dir[]);

/**
 * @brief metodo che controlla se il path è una direcrory
 * @param path path da verificare
 * @return (0) ok (-1) FAIL
 */
int verifica_directory(char *path);

/**
 * @brief metodo che analizza la directory (dirname) e tutte le sue possibili sottodirectory per
 *        cerare i file binari che verrà mandati, appena trovati, ad un thread del threadpool
 *
 * @param dirname nome della directory da cui parte la ricerca
 * @param threadpoll threadpool per la gestione dei thread che eseguiranno il calcolo sui file
 * @param arg
 * @param time
 *
 * @return (-1) FAIL
 */
int processdir(const char dirname[], threadpool_t *threadpool, worker_thread_arg_t *arg, long time, int *n_file);

/**
 * @brief Aggiunge al threadpool il task per inviare al Collector il messaggio di terminazione in seguito ad una ricezione di un segnale
 *        (caso di SIGINT, SIGQUIT, SIGHUP...(tranne SIGURS1))
 * @param workers threadpool dei worker
 *
 * @return (0) OK (-1) FAIL
 */
int signal_mex(threadpool_t *workers);

/**
 * @brief Aggiunge al threadpool il task per chiedere al Collector di stampare i file ricevuti fino ad ora in seguito alla ricezione del segnale SIGURS1.
 *
 * @param workers threadpool dei worker
 *
 * @returns (-1) FAIL (0) OK
 */
int signal_print(threadpool_t *workers);

/**
 * @brief Aggiunge al threadpool il task per inviare al Collector il messaggio di aver completato l'invio di tutti i file.
 *
 * @param workers threadpool dei worker
 *
 * @return (0) OK (-1) FAIL
 */
int done_mex(threadpool_t *workers);

#endif
