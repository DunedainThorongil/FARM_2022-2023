#include "util.h"
#include "master_worker.h"
#include "collector.h"
#include "worker_support.h"
#include "signal.h"

/**
 * @file farm.c
 *
 * @author Alessandra De Lucrezia 565700
 *
 * @brief Implementazione del progetto FARM
 *        Processo principale in cui: (flusso dell'esecuzione senza errori e/o segnali)
 *                  - maschermanto dei segnali (si ignora SIGPIPE)
 *                  - creazione socket
 *                  - fork:
 *                          - processo figlio Collector:
 *                              - listen
 *                              - select
 *                              - stampa risultati
 *                          - processo padre:
 *                              - creazione del thread per le signal
 *                              - creazione del threadpoll
 *                              - creazione del thread master
 *                              - si aspetta la terminazione del figlio
 *                              -  join thread master
 *                              -  join  thread per le signal
 *                              - si distrugge il threadpool
 *                 - si dealloca lo spazio allocato
 *
 */

/* FUNZIONI AUSILIARIE */

/**
 * @brief funzione che viene usata per analizza la CLI ed assegnare i valori alla struttura dati params_master_worker_t.
 *          Nel caso in cui non venga passato nessun valore verranno asseganti i valori di default, ed non essendoci il/i file e/o la directory
 *          il programma terminerà normalmente
 *
 * @param params struttura dati del masterWorker
 * @param argc
 * @param argv
 *
 * @return (0) OK  o (-1) FAIL
 */
static int parsing(params_master_worker_t *params, int argc, char **argv);

/**
 * @brief funzione del thread per la gestione delle signal.
 *        Al ricevimento di un segnale gestito viene mandato un messaggio(di tipo msg_t) sulla connessione socket sempre con l'utilizzo dei thread
 *        del thredpool, con il nome "exit" e valore -1, per tutti i segnali ad eccezzione di SIGURS1 per cui verrà mandato un messaggio(di tipo msg_t) con il nome "print" e valore -1.
 *
 *         Vegono gestite i seguenti segnali:
 *              - SIGINT
 *              - SIGQUIT
 *              - SIGTERM
 *              - SIGHUP
 *              - SIGURS1
 *
 */
static void *signal_function(void *arg);

/* VARIABILI GLOBALI */
static params_master_worker_t *params_worker; /* struttura dati dove vengono inserite le informazioni prese da CLI */

int main(int argc, char **argv)
{

    /* MASCHERAMENTO DEI SEGNALI */

    pthread_t thread_signal; // thread per la gestione dei segnali
    sigset_t set;
    threadpool_t *workers = NULL;

    if (sigfillset(&set) == -1)
    {
        perror("sigfillset");
        goto terminate;
    }
    if (pthread_sigmask(SIG_SETMASK, &set, NULL) != 0)
    {
        perror("pthread_sigmask");
        goto terminate;
    }

    /* Si ignora SIGPIPE */
    struct sigaction s;
    memset(&s, 0, sizeof(s));
    s.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &s, NULL) == -1)
    {
        perror("sigaction");
        goto terminate;
    }

    /* PARSING DEI PARAMENTRI CON CONTROLLO VALORI */

    params_worker = malloc(sizeof(params_master_worker_t));
    if (params_worker == NULL)
    {
        perror("malloc param_worker");
        goto terminate;
    }

    if (parsing(params_worker, argc, argv) == -1)
    {
        goto terminate;
    }

    /* CREAZIONE SOCKET */

    int fd_socket = -1;
    struct sockaddr_un address;

    unlink(SOCKET_NAME);

    memset(&address, '0', sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, SOCKET_NAME, strlen(SOCKET_NAME) + 1);

    if ((fd_socket = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        goto terminate;
    }

    if (bind(fd_socket, (struct sockaddr *)&address, sizeof(address)) == -1)
    {
        perror("bind");
        goto terminate;
    }

    /* FORK */

    pid_t pid = 0;
    int status;
    pid_t pid_padre = getpid();
    if ((pid = fork()) == -1)
    {
        perror("fork");
        goto terminate;
    }

    // fprintf(stdout, "Valore pid %d\n", pid);

    if (pid == 0) /* processo figlio (Collector) */
    {
        int value = collector(fd_socket);
        if (value == -1)
        {
            fprintf(stderr, "Errore nel collector, viene mandato un segnale al padre!\n");
            kill(pid_padre, SIGINT);
        }
    }
    else /* processo padre (MasterWorker) */
    {
        /* CREAZIONE THREAD PER LA GESTIONE DELLE SIGNAL */

        if (pthread_create(&thread_signal, NULL, &signal_function, NULL) != 0)
        {
            perror("pthread_create thread_signal");
            goto terminate;
        }

        /* CREAZIONE THREADPOOL */
        threadpool_t *workers = NULL;
        workers = createThreadPool(params_worker->n_thread, params_worker->queue_len, address);
        if (workers == NULL)
        {
            perror("threadpool create");
            goto terminate;
        }

        params_worker->threadpool = workers;

        /* CREAZIONE THREADMASTER */

        /* Si attende che tutti i thread siano connessi alla socket aperta dal Collector */

        LOCK(&(workers->lock));

        while (workers->connected != workers->numthreads)
        {
            pthread_cond_wait(&(workers->cond_connessi), &(workers->lock));
        }
        // fprintf(stdout, "condizione verificata: TUTTI I THREAD SONO CONNESSI!\n");

        UNLOCK(&(workers->lock));

        pthread_t thread_master;

        if (pthread_create(&thread_master, NULL, &master_worker, (void *)params_worker) != 0)
        {
            perror("pthread_create thread_master");
            goto terminate;
        }
        
        /* CHIUSURA */

        if (pthread_join(thread_master, NULL) != 0)
        {
            perror("pthread_join thread_master");
            goto terminate;
        }
        // fprintf(stdout, "Sto aspettando la terminazione del collector\n");
        /* Si aspetta la terminazione del processo figlio (Collector) controllando lo stato di uscita */
        if (waitpid(pid, &status, 0) == -1)
        {
            perror("waitpid");
            goto terminate;
        }

        if (WIFEXITED(status))
        {
            if (WEXITSTATUS(status) == -1)
            {
                fprintf(stderr, "Errore nel Collector");
            }
            // fprintf(stdout, "valore di ritorno del collector %d\n", WEXITSTATUS(status));
        }

        /* Terminazione del thread per le signal */

        pthread_kill(thread_signal, SIGKILL);
        if (pthread_join(thread_signal, NULL) != 0)
        {
            perror("pthread_join thread_signal");
            goto terminate;
        }

        /* Distruzione del threadpool */
        destroyThreadPool(workers, 1);
    }

    goto clean;

clean:
    kill(getpid(), SIGINT);
    if (pid)
        kill(pid, SIGINT);
    if (fd_socket)
        close(fd_socket);
    if (params_worker->path_directory_name)
        free(params_worker->path_directory_name);
    if (params_worker)
        free(params_worker);
    unlink(SOCKET_NAME);
    return 0;

terminate:
    kill(getpid(), SIGINT);
    if (pid)
        kill(pid, SIGINT);
    if (fd_socket)
        close(fd_socket);
    if (workers != NULL)
        destroyThreadPool(workers, 1);
    if (params_worker->path_directory_name)
        free(params_worker->path_directory_name);
    if (params_worker)
        free(params_worker);
    unlink(SOCKET_NAME);
    return -1;
}

static void *signal_function(void *arg)
{
    sigset_t set;

    int signal = 0;
    if (sigemptyset(&set) == -1)
    {
        perror("sigemptyset");
        return NULL;
    }
    if (sigaddset(&set, SIGHUP) == -1)
    {
        perror("sigaddset (SIGHUP)");
        return NULL;
    }
    if (sigaddset(&set, SIGINT) == -1)
    {
        perror("sigaddset (SIGINT)");
        return NULL;
    }
    if (sigaddset(&set, SIGQUIT) == -1)
    {
        perror("sigaddset (SIGQUIT)");
        return NULL;
    }
    if (sigaddset(&set, SIGTERM) == -1)
    {
        perror("sigaddset (SIGTERM)");
        return NULL;
    }
    if (sigaddset(&set, SIGUSR1) == -1)
    {
        perror("sigaddset (SIGUSR1)");
        return NULL;
    }
    if (pthread_sigmask(SIG_SETMASK, &set, NULL) != 0)
    {
        perror("pthread_sigmask");
        return NULL;
    }
    // fprintf(stdout, "thread_signal attivato\n");

    while (1)
    {
        sigwait(&set, &signal);
        // fprintf(stdout, "Ricevuto segnale %d\n", signal);

        switch (signal)
        {
        case SIGINT:
            fprintf(stdout, "\nRicevuto SIGINT\n");
            signal_mex(params_worker->threadpool);
            return NULL;
            break;

        case SIGUSR1:
            fprintf(stdout, "\nRicevuto SIGUSR1\n");
            signal_print(params_worker->threadpool);
            /* alla ricezione di SIGURS1 il thread continua comunque il suo compito*/

            break;

        case SIGHUP:
            fprintf(stdout, "\nRicevuto SIGHUP\n");
            signal_mex(params_worker->threadpool);
            return NULL;
            break;

        case SIGQUIT:
            fprintf(stdout, "\nRicevuto SIGQUIT\n");
            signal_mex(params_worker->threadpool);
            return NULL;
            break;

        case SIGTERM:
            fprintf(stdout, "\nRicevuto SIGTERM\n");
            signal_mex(params_worker->threadpool);
            return NULL;
            break;

        default:
            break;
        }
    }

    return (void *)0;
}

/**
 * @brief metodo che analizza i valori di input e li assegna alla struttura dati parmas
 *
 * @param params struttura dati in cui verrano salvati i parametri presi da input
 * @param argc
 * @param argv
 *
 * @return (0) ok (-1) FAIL
 */

static int parsing(params_master_worker_t *params, int argc, char **argv)
{
    /* si inizializzano i parametri ai valori di default, nel caso in cui vengono passati da CLI verranno sostituiti */
    params->n_thread = DEFAULT_WORKERS;
    params->path_directory_name = calloc(MAXPATHLENGHT, sizeof(char *));
    if (!params->path_directory_name)
    {
        fprintf(stderr, "calloc fallita params->path_directory_name \n");
        return -1;
    }
    params->queue_len = DEFAULT_QUEUE;
    params->time = DEFAULT_TIME;
    params->argv = argv;
    params->argc = argc;

    int opt = 0;
    while ((opt = getopt(argc, argv, "n:q:t:d:h:")) != -1)
    {
        switch (opt)
        {

        case 'n':
            if (isNumber(optarg, &params->n_thread) != 0)
            {
                params->n_thread = DEFAULT_WORKERS;
            }
            break;

        case 'q':
            if (isNumber(optarg, &params->queue_len) != 0)
            {
                params->queue_len = DEFAULT_QUEUE;
            }

            break;

        case 't':
            if (isNumber(optarg, &params->time) != 0)
            {
                params->time = DEFAULT_TIME;
            }

            break;

        case 'd':
            if (strlen(optarg) > MAXPATHLENGHT)
            {
                fprintf(stderr, "Errore nome directory troppo lungo, non possono essere superati 255 caratteri!\n");

                return (-1);
            }
            else
            {

                if (verifica_directory(optarg) == -1)
                {
                    fprintf(stderr, "Errore '%s' non è una direcory valida! Non verrà analizzata nessuna directory nell'esecuzione!\n", optarg);
                }
                else
                {
                    strcpy(params->path_directory_name, optarg);
                }
            }
            break;

        case 'h':
            fprintf(stdout, "Usage: %s  <-n (nthread)> <-q (qlen)> <-t (delay)> <file list> <-d (directory-name)>\n", argv[0]);
            return (-1);
            break;

        case '?':
            printf("Opzione '-%c' non e' riconosciuta\n", optopt);
            fprintf(stderr, "Usage: %s  <-n (nthread)> <-q (qlen)> <-t (delay)> <file list> <-d (directory-name)>\n", argv[0]);
            return -1;
            break;

        default:
            fprintf(stderr, "Usage: %s  <-n (nthread)> <-q (qlen)> <-t (delay)> <file list> <-d (directory-name)>\n", argv[0]);
            return (-1);
            break;
        }
    }

    if (strlen(params->path_directory_name) == 0)
    {
        fprintf(stderr, "Non è stata passata nessuna directory\n");
    }

    return 0;
}
