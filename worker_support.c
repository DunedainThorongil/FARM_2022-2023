#include "util.h"
#include "master_worker.h"
#include "worker_support.h"
#include "collector.h"

static int uscita = 0;

int processdir(const char dirname[], threadpool_t *threadpool, worker_thread_arg_t *arg, long time, int *n_file)
{
  DIR *dir;

  if (!dirname)
  {
    errno = EINVAL;
    return -1;
  }

  if (chdir(dirname) == -1)
  {
    return 0;
  }

  if ((dir = opendir(".")) == NULL)
  {
    return -1;
  }
  else
  {
    struct dirent *file;
    struct stat info;
    while ((errno = 0, file = readdir(dir)) != NULL)
    {
      if (uscita == 0) // non è stata ricevuto nessun segnale di uscita
      {
        if (stat(file->d_name, &info) == 0)
        {
          if (S_ISREG(info.st_mode))
          {
            if (file_dat(file->d_name) == 0)
            {
              char tmp[MAXPATHLENGHT];
              char *path = realpath(file->d_name, tmp);
              arg = malloc(sizeof(worker_thread_arg_t));
              memset((*arg).file, 0, MAXPATHLENGHT * sizeof(char));
              strncpy((*arg).file, path, strlen(path));
              // fprintf(stdout, "Questo file '%s' verrà mandato ad un thread %s\n\n", path, file->d_name);
              if (addToThreadPool(threadpool, worker, (void *)arg) == -1)
              {
                perror("addToThreadPool");
                free(path);
                closedir(dir);
                return -1;
              }
              (*n_file)++;
              sleep(time / 1000);
            }
          }
          else if (dots_only(file->d_name) == -1)
          {
            if (S_ISDIR(info.st_mode))
            {
              if (processdir(file->d_name, threadpool, arg, time, n_file) != 0)
              {
                if (chdir("..") == -1)
                  return -1;
              }
            }
          }
        }
      }
      else
      { // ricevuto un signale di uscita
        closedir(dir);
        return 1;
      }
    }
    if (errno != 0)
    {
      return -1;
    }

    closedir(dir);
  }

  return 1;
}

int verifica_directory(char *path)
{
  errno = 0;
  struct stat info;

  if (stat(path, &info) == 0)
  {
    if (S_ISDIR(info.st_mode))
    {
      return 0;
    }
    return -1;
  }
  else
  {
    perror("stat");
    return -1;
  }
  return -1;
}

int dots_only(const char dir[])
{
  if (!dir)
    return -1;
  int l = strlen(dir);

  if (l > 0 && dir[l - 1] == '.')
    return 0;

  return -1;
}

int file_dat(char *str)
{
  if (str == NULL)
  {
    return -1;
  }
  char *dat = ".dat";

  if (strlen(str) < strlen(dat))
  {
    return -1;
  }
  int l_dat = strlen(dat);
  int l_tmp = strlen(str);

  int i = 1;

  /* vengono confrontate le due stringhe partendo dalla fine, se viene trovato un valore diverso, si ritorna -1 */

  while (i < l_dat + 1)
  {
    // printf(" %d di dat %c\n", i, dat[len1 - i]);
    // printf(" %d di str2 %c\n", i, str2[len2 - i]);

    if (dat[l_dat - i] != str[l_tmp - i])
    {
      return -1;
    }
    i++;
  }

  return 0;
}

int signal_mex(threadpool_t *workers)
{
  uscita = 1;
  if (workers == NULL)
  {
    return -1;
  }
  /* Si prepare la struttura dati da inviare */
  /* Viene inviato un messaggio al collector per terminare (caso di SIGINT, SIGQUIT, SIGHUP...(tranne SIGURS1)) */
  worker_thread_arg_t *signal_args = malloc(sizeof(worker_thread_arg_t));
  memset((*signal_args).file, 0, MAXPATHLENGHT * sizeof(char));
  strncpy((*signal_args).file, "signal", strlen("signal") + 1);
  if (addToThreadPool(workers, worker, (void *)signal_args) < 0)
  { // errore interno
    fprintf(stderr, "ERROR, addToThreadPool\n");
  }

  return 0;
}

int signal_print(threadpool_t *workers)
{
  if (workers == NULL)
  {
    return -1;
  }
  // si prepare la struttura dati da inviare
  worker_thread_arg_t *done = malloc(sizeof(worker_thread_arg_t));
  memset((*done).file, 0, MAXPATHLENGHT * sizeof(char));
  strncpy((*done).file, "print", strlen("print") + 1);

  /* un thread del threadpool metterà come valore di msg.conto uguale a -1 e invierà msg_t sulla socket*/

  if (addToThreadPool(workers, worker, (void *)done) < 0)
  { // errore interno
    fprintf(stderr, "FATAL ERROR, addToThreadPool\n");
    return -1;
  }
  return 0;
}

int done_mex(threadpool_t *workers)
{
  if (workers == NULL)
  {
    return -1;
  }
  // si prepare la struttura dati da inviare
  worker_thread_arg_t *done = malloc(sizeof(worker_thread_arg_t));
  memset((*done).file, 0, MAXPATHLENGHT * sizeof(char));
  strncpy((*done).file, "done", strlen("done") + 1);
  /* un thread del threadpool metterà come valore di msg.conto uguale al valore che verrà calcolato e invierà msg_t sulla socket*/

  if (addToThreadPool(workers, worker, (void *)done) < 0)
  { // errore interno
    fprintf(stderr, "FATAL ERROR, addToThreadPool\n");
    return -1;
  }
  return 0;
}