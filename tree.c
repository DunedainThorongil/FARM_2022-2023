#include "tree.h"
/**
 * @file tree.c
 *
 * @brief file contenente la definizione dei metodi base per gestire un albero binario con inserimento ordinato con nodi di tipo msg_t
 *
 * @author Alessandra De Lucrezia 565700
 *
 */

static char *currDir;

Nodo *creaNodo(msg_t chiave)
{
    Nodo *nuovoNodo = (Nodo *)malloc(sizeof(Nodo));
    nuovoNodo->chiave = chiave;
    nuovoNodo->sinistra = NULL;
    nuovoNodo->destra = NULL;
    return nuovoNodo;
}

// Funzione per inserire un nodo nell'albero
Nodo *inserisci(Nodo *radice, msg_t chiave)
{
    if (radice == NULL)
    {
        return creaNodo(chiave);
    }
    if (chiave.conto < radice->chiave.conto)
    {
        radice->sinistra = inserisci(radice->sinistra, chiave);
    }
    else if (chiave.conto > radice->chiave.conto)
    {
        radice->destra = inserisci(radice->destra, chiave);
    }
    return radice;
}

void stampaInOrdine(Nodo *radice)
{
    if (radice != NULL)
    {
        stampaInOrdine(radice->sinistra);
        printf("%ld %s\n", radice->chiave.conto, radice->chiave.pathfile);
        stampaInOrdine(radice->destra);
    }
}

void liberaAlbero(Nodo *radice)
{
    if (radice != NULL)
    {
        liberaAlbero(radice->sinistra);
        liberaAlbero(radice->destra);
        free(radice);
    }
    if (currDir != NULL)
        free(currDir);
}

int short_path(msg_t *msg, char *currDir)
{

    /* si prende il nome del file*/
    int len_msg = strlen(msg->pathfile) + 1;
    // fprintf(stdout, "lunghezza tmp %d\n", len_tmp);

    /* la stringa tmp sarà sicuramente più lunga della stringa currDir, visto che è stato assunto che le directory inserite nella CLI
     sono all'intenro della directory del progetto e non altrove */
    int i = 0;
    while (i < len_msg)
    {
        if ((msg->pathfile)[i] != currDir[i])
        {
            break;
        }
        i++;
    }
    // fprintf(stdout, "valore di i %d\n", i);
    // fprintf(stdout, "valore del primo carattere diverso %c\n", tmp[i]);

    /* copio la restante parte di tmp, da i+1 fino a len_tmp, che poi verrà memorizzata in msg.pathfile */
    char *filename = calloc((len_msg - i + 1), sizeof(char *));
    int n = 0;
    for (int j = i + 1; j < len_msg; j++)
    {
        filename[n] = (msg->pathfile)[j];
        n++;
    }
    int l = strlen(filename);
    filename[l] = '\0'; // aggiunta del null byte finale

    // fprintf(stdout, "\n\t\tNome file ricevuto %s\n\n", filename);

    memset(msg->pathfile, 0, MAXPATHLENGHT * sizeof(char));
    strncpy(msg->pathfile, filename, strlen(filename) + 1);
    // fprintf(stdout, "\t\t\tmsg.pathfile = %s\n\n", msg.pathfile);

    free(filename);

    return 0;
}