#include "util.h"
#include "collector.h"
#include "tree.h"

/**
 * @file collector.c
 *
 * @author Alessandra De Lucrezia 565700
 *
 * @brief File con l'implementazione del processo figlio.
 *        Il collector dopo la listen usando la select seleziona i canali pronti tramite fd
 *        e quando ha ricevuto tutti i file li stampa e termina.
 *        Nel caso di ricezione di segnali come SIGINT, SIGQUIT, SIGTERM, SIGHUP stampa i file ricevuti fino a quel momemento e termina
 *        Mentre nel caso in cui riceve SIGURS1 stampa i valori ricevuti fino a quel momento e poi continua la sua esecuzione
 *
 */

int collector(int ssfd)
{
    /* Salvataggio directory corrente */
    char *currDir = (char *)malloc(MAXPATHLENGHT * sizeof(char));
    memset(currDir, 0, MAXPATHLENGHT * sizeof(char));
    if (getcwd(currDir, MAXPATHLENGHT * sizeof(char)) == NULL)
    {
        perror("getcdw");
        goto terminate;
    }

    /* albero in cui verranno salvati i messaggi ricevuti sulla connessione socket */
    Nodo *radice = NULL;

    int uscita = 0; /* se = 1 è stato ricevuto un segnale di terminazione */

    int n_f_da_ricevere = 0; /* gli viene assegnato il valore quando si riceve il messaggio "don"e con il numero di file tot da ricevere prima di chiudere */
    int n_file_ricevuti = 0; /* indica il numero dei file ricevuti */

    int fd_c;
    int fd_num = 0; /* max fd attivo */
    int fd;         /* indice per verificare risultati select */

    fd_set set;   /* l’insieme dei file descriptor attivi */
    fd_set rdset; /* insieme fd attesi in lettura */
    int nread;    /* numero caratteri letti */

    if (listen(ssfd, SOMAXCONN) == -1)
    {
        perror("listen");
        goto terminate;
    }

    /* mantengo il massimo indice di descrittore attivo in fd_num */
    if (ssfd > fd_num)
        fd_num = ssfd;

    FD_ZERO(&set);
    FD_SET(ssfd, &set);

    while (!uscita)
    {
        rdset = set; /* preparo maschera per la select */
        if (select(fd_num + 1, &rdset, NULL, NULL, NULL) == -1)
        {
            perror("select");
            goto terminate;
        }

        /* select OK */
        for (fd = 0; fd <= fd_num; fd++)
        {
            if (FD_ISSET(fd, &rdset))
            {
                if (fd == ssfd)
                { /* sock connect pronto */

                    // fprintf(stdout, "\t\t\tsono in fd==ssfd\n");
                    if ((fd_c = accept(ssfd, NULL, 0)) == -1)
                    {
                        perror("accept");
                        goto terminate;
                    }
                    FD_SET(fd_c, &set);
                    if (fd_c > fd_num)
                        fd_num = fd_c;
                }
                else
                {
                    /* sock I/0 pronto */
                    fd_c = fd;
                    msg_t msg;

                    memset(&msg, '\0', MAXPATHLENGHT);
                    if ((nread = readn(fd_c, &msg, sizeof(msg_t))) == -1)
                    {
                        perror("readn");
                        goto terminate;
                    }

                    if (nread != 0) // se == 0 leggo EOF
                    {
                        if (strcmp(msg.pathfile, "done") == 0)
                        {
                            n_f_da_ricevere = msg.conto;
                            //  fprintf(stdout, "\n\n\t\t\tHo ricevuto DONE\n");

                            /* verifica se il collector ha ricevuto tutti i file che doveva ricevere */
                            if (n_file_ricevuti == msg.conto)
                            {
                                uscita = 1;

                            } /* ELSE continua a ricevere i file dai thread*/
                        }
                        else if (strcmp(msg.pathfile, "signal") == 0)
                        {
                            // fprintf(stdout, "HO RICEVUTO signal\n");
                            uscita = 1;
                        }
                        else if (strcmp(msg.pathfile, "print") == 0)
                        {
                            // fprintf(stdout, "HO RICEVUTO print\n");
                            // print_list(list);
                            stampaInOrdine(radice);
                        }
                        else
                        {
                            // fprintf(stdout, "valore di msg.pathfile %s\n", msg.pathfile);

                            /* Viene inserita la struttura dati ricevuta nell'albero in maniera ordinato */
                            if (short_path(&msg, currDir) != 0)
                            {
                                goto terminate;
                            }
                            radice = inserisci(radice, msg);
                            n_file_ricevuti++;
                            //goto terminate;
                            /* Controllo se il collector ha ricevuti tutti i file, se si può terminare */
                            if (n_f_da_ricevere == n_file_ricevuti)
                            {
                                uscita = 1;
                            }
                        }
                    }
                    else if (nread == -1)
                    {
                        perror("readn");
                        goto terminate;
                    }
                }
            }
        }
    }

    //  fprintf(stdout, "\n\t\tNumero di file ricevuti dal collector %d\n", list->size);

    /* Si stampa la lista */
    stampaInOrdine(radice);

    /* PULIZIA */
    goto clean;

clean:

    liberaAlbero(radice);
    free(currDir);
    FD_CLR(fd_c, &set);
    close(fd_c);
    unlink(SOCKET_NAME);
    return 0;

terminate:
    if (radice)
        liberaAlbero(radice);
    if (currDir)
    {
        free(currDir);
    }
    unlink(SOCKET_NAME);
    return -1;
}