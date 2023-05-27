#include "master_worker.h"
#include "worker_support.h"
#include "threadpool.h"
#include "collector.h"

/**
 * @file master_worker.c
 *
 * @author Alessandra De Lucrezia 565700
 *
 * @brief File contente l'implementazione del master worker e del metodo dei thread del threadpool.
 *        Il masterWorker si occupa di passare i file binari ad i thread e poi loro si occuperanno di aprire i file ed eseguire il calcolo ed di inviare
 *        la struttura dati msg_t contentente il nome del file e il valore calcolato al Collector tramite la connessione socket stabilita al momento della creazione
 *        del threapool (creazione stabile per ogni thread)
 *
 */

/* VARIABILI GLOBALI */
static int n_file = 0;    /* Numero di file inviati al collector */
static int exit_flag = 0; /* Può essere settata ad 1 per due motivi:
                           * 1) la connessione è stata chiusa dal Collector per un errore e quindi i thread alla write avranno 'brokenPipe'
                           * 2) è stato inviato il segnale di uscita dal thread_signal
                           */
void *master_worker(void *params)
{

    params_master_worker_t *p = (params_master_worker_t *)params;
    worker_thread_arg_t *arg = NULL;
    // fprintf(stdout, "ThreadMaster attivato\n");

    // fprintf(stdout, "valore di argc  in workec.c %d\n", p->argc);

    /* Vengono controllati tutti gli argomenti presi da argv e si cercano i file.dat da mandare */

    for (int i = 1; (i < p->argc) && (exit_flag == 0); i++)
    {
        struct stat info;
        if (stat(p->argv[i], &info) == 0)
        {
            if (S_ISREG(info.st_mode))
            {
                if (file_dat(p->argv[i]) == 0)
                {
                    /* Si prende il path assoluto */
                    char tmp[MAXPATHLENGHT];
                    char *path = realpath(p->argv[i], tmp);
                    if (!path)
                    {
                        fprintf(stderr, "error realpath\n");

                        return (void *)0;
                    }
                    arg = malloc(sizeof(worker_thread_arg_t));
                    memset((*arg).file, 0, MAXPATHLENGHT * sizeof(char));
                    strncpy((*arg).file, path, strlen(path));
                    // fprintf(stdout, "file trovato '%s', lo mando ad un thread %s\n", p->argv[i], path);
                    // fprintf(stdout, "valore di addtothread %d\n", errcode);
                    if (addToThreadPool(p->threadpool, worker, (void *)arg) == -1)
                    {
                        perror("addToThreadPool");
                        free(arg);
                        free(path);
                        return (void *)0;
                    }
                    n_file++;
                    // tra un task ed un altro si aspetta il tempo indicato a linea di comando, di defautl è 0
                    sleep(p->time / 1000);
                }
            }
        }
    }
    /* Viene processata la directory indicata al momento della compilazione e tutte le sue sottodirectory se non è stato
     * ricevuto il segnale di uscita
     */

    // fprintf(stdout, "directory '%s'\n", p->path_directory_name);
    if (exit_flag == 0)
    {
        if (p->path_directory_name != NULL)
        {
            if (processdir(p->path_directory_name, p->threadpool, (void *)arg, p->time, &n_file) == -1)
            {
                fprintf(stderr, "Errore processdir!\n");
            }
        }
    }

    // fprintf(stdout, "Fine lettura file da argv, il totale dei file è %d\n", n_file);
    // fprintf(stdout, "\n\nNumero di file inviati %d\n\n", n_file);

    // Viene inviato il messaggio di fine ("done", con conto = n_file) se non è stato ricevuto nessun segnale di uscita *
    if (exit_flag == 0)
    {
        done_mex(p->threadpool);
    }

    // fprintf(stdout, "sono il master e ho finito\n");
    return (void *)0;
}

void worker(void *param)
{
    // fprintf(stdout, "Sono in worker\n");
    worker_thread_arg_t *arg = (worker_thread_arg_t *)param;

    int fd = (*arg).fd_skt;

    // fprintf(stdout, "Sto per eseguire il calcolo del file '%s' %d\n", arg->file, arg->fd_skt);
    msg_t msg;
    memset(&msg, 0, sizeof(msg_t));
    char *filename = (char *)malloc(sizeof(char) * MAXPATHLENGHT);
    memset(filename, 0, MAXPATHLENGHT * sizeof(char));
    strncpy(filename, (*arg).file, strlen((*arg).file));

    if (strcmp(filename, "done") == 0)
    {
        msg.conto = n_file;
        strncpy(msg.pathfile, filename, strlen(filename));
        // fprintf(stdout, "\n\nINVIO DONE %ld\n\n", msg.conto);

        if (writen(fd, &msg, sizeof(msg)) == -1)
        {
            if (errno == EPIPE) /* viene gestito nello specifico il caso di 'broken pipe' che avviene quando c'è un errore nel collector
                                 * con la conseguenza della chiusura della comunicazione socket tra il collector e i worker
                                 **/
            {
                perror("writen done");
                exit_flag = 1;
            }
            else
            {
                perror("writen done");
                _exit(EXIT_FAILURE);
            }
        }
    }
    else if (strcmp(filename, "signal") == 0)
    {
        exit_flag = 1;
        strncpy(msg.pathfile, filename, strlen(filename));
        msg.conto = -1;
        if (writen(fd, &msg, sizeof(msg)) == -1)
        {
            if (errno == EPIPE) /* viene gestito nello specifico il caso di 'broken pipe' che avviene quando c'è un errore nel collector
                                 * con la conseguenza della chiusura della comunicazione socket tra il collector e i worker
                                 **/
            {
                perror("writen worker signal");
                exit_flag = 1;
            }
            else
            {
                perror("writen worker signal");
                _exit(EXIT_FAILURE);
            }
        }
    }
    else if (strcmp(filename, "print") == 0)
    {
        strncpy(msg.pathfile, filename, strlen(filename));
        msg.conto = 0;

        if (writen(fd, &msg, sizeof(msg)) == -1)
        {
            if (errno == EPIPE) /* viene gestito nello specifico il caso di 'broken pipe' che avviene quando c'è un errore nel collector
                                 * con la conseguenza della chiusura della comunicazione socket tra il collector e i worker
                                 **/
            {
                perror("writen worker print");
                exit_flag = 1;
            }
            else
            {
                perror("writen worker print");
                _exit(EXIT_FAILURE);
            }
        }
    }
    else
    {
        FILE *file = NULL;
        file = fopen(filename, "rb");
        if (file == NULL)
        {
            perror("fopen");
            msg.conto = -1; /* nel caso di errore da fopen il file viene comunque inviato con il valore = -1 */
        }
        else
        {
            // fprintf(stdout, "Sto per eseguire il calcolo del file '%s' %d\n", arg->file, arg->fd_skt);

            long sum = 0;
            int i = 0;
            long num = 0;
            /* leggo dal file un numero long alla volta */
            while (fread(&num, sizeof(long), 1, file) == 1)
            {
                sum += num * i;
                i++;
            }
            // fprintf(stdout, "\n\nCalcolo eseguito del file %s : %ld\n\n", arg->file, calcolo(arg->file));
            msg.conto = sum;
        }

        strncpy(msg.pathfile, filename, strlen(filename) + 1);
        /* Al Collector non viene mandato tutto il path, ma solo il path dalla directory principale al nome del file */

        // fprintf(stdout, "\n\nInvio il file %s valore %ld\n\n", msg.pathfile, msg.conto);

        if (writen(fd, &msg, sizeof(msg)) == -1)
        {
            if (errno == EPIPE) /* viene gestito nello specifico il caso di 'broken pipe' che avviene quando c'è un errore nel collector
                                 * con la conseguenza della chiusura della comunicazione socket tra il collector e i worker
                                 **/
            {
                perror("writen worker file");
                exit_flag = 1;
            }
            else
            {
                perror("writen worker file");
                _exit(EXIT_FAILURE);
            }
        }

        // fprintf(stdout, "%d\n", n_file);
        if (msg.conto != -1)
        {
            if (fclose(file) != 0)
            {
                perror("fclose");
            }
        }
    }
    goto clean;

clean:
    if (arg)
        free(arg);
    if (filename)
        free(filename);
}
