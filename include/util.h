/**
 * @file    util.h
 *
 * @author  Recuperato dalla correzione degli assegnamenti.
 *          Eventuali modifihe sono a cura di Alessandra De Lucrezia 565700
 *
 * @brief   header di utility.
 *          Contiene una serie di macro per la gestione degli errori in seguito a System Call, chiamate su lock e codition variables.
 *
 */

#if !defined(UTIL_H)
#define UTIL_H

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <getopt.h>
#include <assert.h>
#include <libgen.h>

#include <sys/select.h>

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <fcntl.h>

#include <pthread.h>
#include <errno.h>
#include <wait.h>
#include <signal.h>

#include <math.h>

#define SOCKET_NAME "farm.sck"

#if !defined(BUFSIZE)
#define BUFSIZE 256
#endif

#if !defined(MAXPATHLENGHT)
#define MAXPATHLENGHT 255
#endif

#if !defined(EXTRA_LEN_PRINT_ERROR)
#define EXTRA_LEN_PRINT_ERROR 512
#endif

#define UNIX_PATH_MAX 108

#define SYSCALL_EXIT(name, r, sc, str, ...) \
  if ((r = sc) == -1)                       \
  {                                         \
    perror(#name);                          \
    int errno_copy = errno;                 \
    print_error(str, __VA_ARGS__);          \
    exit(errno_copy);                       \
  }

#define SYSCALL_PRINT(name, r, sc, str, ...) \
  if ((r = sc) == -1)                        \
  {                                          \
    perror(#name);                           \
    int errno_copy = errno;                  \
    print_error(str, __VA_ARGS__);           \
    errno = errno_copy;                      \
  }

#define SYSCALL_RETURN(name, r, sc, str, ...) \
  if ((r = sc) == -1)                         \
  {                                           \
    perror(#name);                            \
    int errno_copy = errno;                   \
    print_error(str, __VA_ARGS__);            \
    errno = errno_copy;                       \
    return r;                                 \
  }

#define CHECK_EQ_EXIT(name, X, val, str, ...) \
  if ((X) == val)                             \
  {                                           \
    perror(#name);                            \
    int errno_copy = errno;                   \
    print_error(str, __VA_ARGS__);            \
    exit(errno_copy);                         \
  }

#define CHECK_NEQ_EXIT(name, X, val, str, ...) \
  if ((X) != val)                              \
  {                                            \
    perror(#name);                             \
    int errno_copy = errno;                    \
    print_error(str, __VA_ARGS__);             \
    exit(errno_copy);                          \
  }

/**
 * \brief Procedura di utilita' per la stampa degli errori
 *
 */
static inline void print_error(const char *str, ...)
{
  const char err[] = "ERROR: ";
  va_list argp;
  char *p = (char *)malloc(strlen(str) + strlen(err) + EXTRA_LEN_PRINT_ERROR);
  if (!p)
  {
    perror("malloc");
    fprintf(stderr, "FATAL ERROR nella funzione 'print_error'\n");
    return;
  }
  strcpy(p, err);
  strcpy(p + strlen(err), str);
  va_start(argp, str);
  vfprintf(stderr, p, argp);
  va_end(argp);
  free(p);
}

#define LOCK(l)                              \
  if (pthread_mutex_lock(l) != 0)            \
  {                                          \
    fprintf(stderr, "ERRORE FATALE lock\n"); \
    pthread_exit((void *)EXIT_FAILURE);      \
  }
#define LOCK_RETURN(l, r)                    \
  if (pthread_mutex_lock(l) != 0)            \
  {                                          \
    fprintf(stderr, "ERRORE FATALE lock\n"); \
    return r;                                \
  }

#define UNLOCK(l)                              \
  if (pthread_mutex_unlock(l) != 0)            \
  {                                            \
    fprintf(stderr, "ERRORE FATALE unlock\n"); \
    pthread_exit((void *)EXIT_FAILURE);        \
  }
#define UNLOCK_RETURN(l, r)                    \
  if (pthread_mutex_unlock(l) != 0)            \
  {                                            \
    fprintf(stderr, "ERRORE FATALE unlock\n"); \
    return r;                                  \
  }
#define WAIT(c, l)                           \
  if (pthread_cond_wait(c, l) != 0)          \
  {                                          \
    fprintf(stderr, "ERRORE FATALE wait\n"); \
    pthread_exit((void *)EXIT_FAILURE);      \
  }
/* ATTENZIONE: t e' un tempo assoluto! */
#define TWAIT(c, l, t)                                                \
  {                                                                   \
    int r = 0;                                                        \
    if ((r = pthread_cond_timedwait(c, l, t)) != 0 && r != ETIMEDOUT) \
    {                                                                 \
      fprintf(stderr, "ERRORE FATALE timed wait\n");                  \
      pthread_exit((void *)EXIT_FAILURE);                             \
    }                                                                 \
  }
#define SIGNAL(c)                              \
  if (pthread_cond_signal(c) != 0)             \
  {                                            \
    fprintf(stderr, "ERRORE FATALE signal\n"); \
    pthread_exit((void *)EXIT_FAILURE);        \
  }
#define BCAST(c)                                  \
  if (pthread_cond_broadcast(c) != 0)             \
  {                                               \
    fprintf(stderr, "ERRORE FATALE broadcast\n"); \
    pthread_exit((void *)EXIT_FAILURE);           \
  }
static inline int TRYLOCK(pthread_mutex_t *l)
{
  int r = 0;
  if ((r = pthread_mutex_trylock(l)) != 0 && r != EBUSY)
  {
    fprintf(stderr, "ERRORE FATALE unlock\n");
    pthread_exit((void *)EXIT_FAILURE);
  }
  return r;
}

/** Evita letture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la lettura da fd leggo EOF
 *   \retval size se termina con successo
 */
static inline int readn(long fd, void *buf, size_t size)
{
  size_t left = size;
  int r;
  char *bufptr = (char *)buf;
  while (left > 0)
  {
    if ((r = read((int)fd, bufptr, left)) == -1)
    {
      if (errno == EINTR)
        continue;
      return -1;
    }
    if (r == 0)
      return 0; // EOF
    left -= r;
    bufptr += r;
  }
  return size;
}

/** Evita scritture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la scrittura la write ritorna 0
 *   \retval  1   se la scrittura termina con successo
 */
static inline int writen(long fd, void *buf, size_t size)
{
  size_t left = size;
  int r;
  char *bufptr = (char *)buf;
  while (left > 0)
  {
    if ((r = write((int)fd, bufptr, left)) == -1)
    {
      if (errno == EINTR)
        continue;
      return -1;
    }
    if (r == 0)
      return 0;
    left -= r;
    bufptr += r;
  }
  return 1;
}

/**
 * @brief Funzione che converte una stringa in numero
 *
 * @param string stringa che si vuole convertire in intero
 * @param n variabile all'interno della quale andare ad inserire il numero
 *
 * @return (0) OK (1) FAIL (2) OVERFLOW/UNDERFLOW
 *
 */
static inline int isNumber(const char *s, long *n)
{

  if (s == NULL)
    return 1;

  if (strlen(s) == 0)
    return 1;

  char *e = NULL;
  errno = 0;
  long val = strtol(s, &e, 10);

  if (errno == ERANGE)
    return 2; // overflow/underflow

  if (e != NULL && *e == (char)0)
  {
    if (val < 0)
    {
      fprintf(stdout, "I valori negativi non sono ammessi, verrà preso il valore assoluto del numero inserito!\n\n");
      val = labs(val);
    }
    *n = val;
    return 0; // successo
  }

  return 1; // non e' un numero
}

#endif /* _UTIL_H */
