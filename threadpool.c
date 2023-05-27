/**
 * @file threadpool.c
 *
 * @author File preso dalla soluzione dell'ottavo assegnamento.
 *         Le modifiche sono a cura di Alessandra De Lucrezia 565700
 *
 * @brief File di implementazione dell'interfaccia Threadpool
 */

#include <util.h>
#include "threadpool.h"
#include "master_worker.h"
#include "worker_support.h"

struct sockaddr_un sa;

/**
 * @function void *threadpool_thread(void *threadpool)
 * @brief funzione eseguita dal thread worker che appartiene al pool
 */
static void *workerpool_thread(void *threadpool)
{
    // fprintf(stdout, "sono in workerpool_thread\n\n");
    threadpool_t *pool = (threadpool_t *)threadpool; // cast
    taskfun_t task;                                  // generic task
    pthread_t self = pthread_self();
    int myid = -1;
    int csfd;

    SYSCALL_EXIT("socket", csfd, socket(AF_UNIX, SOCK_STREAM, 0), "socket: errno=%d\n", errno);
    while (connect(csfd, (struct sockaddr *)&sa, sizeof(sa)) == -1 && !pool->exiting)
    {
        sleep(0.01);
    }

    do
    {
        for (int i = 0; i < pool->numthreads; ++i)
            if (pthread_equal(pool->threads[i], self))
            {
                myid = i;
                break;
            }
    } while (myid < 0);

    // fprintf(stdout, "connesso thread %d con csfd %d\n", myid, csfd);

    // prendo la lock sul thread pool ed incremento il numero di thread connessi
    LOCK_RETURN(&(pool->lock), NULL);
    (pool->connected)++;

    // avviso i thread in attesa sulla condizione connessi

    if (pool->connected == pool->numthreads)
    {
        int errcode = 0;
        if ((errcode = pthread_cond_signal(&(pool->cond_connessi))) != 0)
        {
            errno = errcode;
            perror("pthread_cond_signal cond_connessi");
            return NULL;
        }
    }

    for (;;)
    {
        // in attesa di un messaggio, controllo spurious wakeups.
        while ((pool->count == 0) && (!pool->exiting))
        {
            pthread_cond_wait(&(pool->cond), &(pool->lock));
        }

        if (pool->exiting > 1)
            break; // exit forzato, esco immediatamente
        // devo uscire ma ci sono messaggi pendenti
        if (pool->exiting == 1 && !pool->count)
            break;

        // nuovo task
        task.fun = pool->pending_queue[pool->head].fun;
        task.arg = pool->pending_queue[pool->head].arg;

        pool->head++;
        pool->count--;
        pool->head = (pool->head == abs(pool->queue_size)) ? 0 : pool->head;

        pool->taskonthefly++;
        int errcode = 0;
        if ((pool->count < pool->queue_size))
        {
            if ((errcode = pthread_cond_broadcast(&(pool->cond_coda))) != 0)
            {
                errno = errcode;
                perror("pthread_cond_signal cond_connessi");
                return NULL;
            }
        }
        UNLOCK_RETURN(&(pool->lock), NULL);

        worker_thread_arg_t *w_args = task.arg;
        // fprintf(stdout, "valore di csfd %d del thread %d\n", csfd, myid);
        w_args->fd_skt = csfd;

        // eseguo la funzione
        (*(task.fun))(task.arg);

        LOCK_RETURN(&(pool->lock), NULL);
        pool->taskonthefly--;
        int r;
        if ((r = pthread_cond_broadcast(&(pool->condFull))) != 0)
        {
            UNLOCK_RETURN(&(pool->lock), NULL);
            errno = r;
            // fprintf(stderr, "signal\n");
            return NULL;
        }
    }
    UNLOCK_RETURN(&(pool->lock), NULL);

    // fprintf(stderr, "thread %d exiting\n", myid);
    return NULL;
}

static int freePoolResources(threadpool_t *pool)
{
    if (pool->threads)
    {
        free(pool->threads);
        free(pool->pending_queue);

        pthread_mutex_destroy(&(pool->lock));
        pthread_cond_destroy(&(pool->cond));
        pthread_cond_destroy(&(pool->cond_connessi));
        pthread_cond_destroy(&(pool->condFull));
        pthread_cond_destroy(&(pool->cond_coda));
    }
    free(pool);
    return 0;
}

threadpool_t *createThreadPool(int numthreads, int pending_size, struct sockaddr_un sockAddr)
{
    // fprintf(stdout, "sono in create threadpool\n");
    if (numthreads <= 0 || pending_size < 0)
    {
        errno = EINVAL;
        return NULL;
    }

    threadpool_t *pool = (threadpool_t *)malloc(sizeof(threadpool_t));
    if (pool == NULL)
        return NULL;

    // condizioni iniziali
    pool->numthreads = 0;
    pool->taskonthefly = 0;
    pool->queue_size = (pending_size == 0 ? -1 : pending_size);
    pool->head = pool->tail = pool->count = 0;
    pool->exiting = 0;
    pool->connected = 0;

    sa = sockAddr;

    /* Allocate thread and task queue */
    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * numthreads);
    if (pool->threads == NULL)
    {
        free(pool);
        return NULL;
    }
    pool->pending_queue = (taskfun_t *)malloc(sizeof(taskfun_t) * abs(pool->queue_size));
    if (pool->pending_queue == NULL)
    {
        free(pool->threads);
        free(pool);
        return NULL;
    }
    if ((pthread_mutex_init(&(pool->lock), NULL) != 0) ||
        (pthread_cond_init(&(pool->cond_connessi), NULL) != 0) ||
        (pthread_cond_init(&(pool->cond), NULL) != 0) ||
        pthread_cond_init(&(pool->condFull), NULL) ||
        pthread_cond_init(&(pool->cond_coda), NULL))
    {
        free(pool->threads);
        free(pool->pending_queue);
        free(pool);
        return NULL;
    }
    for (int i = 0; i < numthreads; i++)
    {
        if (pthread_create(&(pool->threads[i]), NULL, workerpool_thread, (void *)pool) != 0)
        {
            /* errore fatale, libero tutto forzando l'uscita dei threads */
            destroyThreadPool(pool, 1);
            errno = EFAULT;
            return NULL;
        }
        pool->numthreads++;
    }
    return pool;
}

int destroyThreadPool(threadpool_t *pool, int force)
{
    // fprintf(stdout, "sono in destroy threadpool\n");

    if (pool == NULL || force < 0)
    {
        errno = EINVAL;
        return -1;
    }

    LOCK_RETURN(&(pool->lock), -1);

    pool->exiting = 1 + force;

    if (pthread_cond_broadcast(&(pool->cond)) != 0)
    {
        UNLOCK_RETURN(&(pool->lock), -1);
        errno = EFAULT;
        return -1;
    }
    UNLOCK_RETURN(&(pool->lock), -1);

    for (int i = 0; i < pool->numthreads; i++)
    {
        if (pthread_join(pool->threads[i], NULL) != 0)
        {
            errno = EFAULT;
            UNLOCK_RETURN(&(pool->lock), -1);
            return -1;
        }
    }
    freePoolResources(pool);
    return 0;
}

int addToThreadPool(threadpool_t *pool, void (*f)(void *), void *arg)
{
    // fprintf(stdout, "Sono in addToThreadPool\n");

    if (pool == NULL || f == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    LOCK_RETURN(&(pool->lock), -1);
    int queue_size = abs(pool->queue_size);
    int nopending = (pool->queue_size == -1); // non dobbiamo gestire messaggi pendenti

    // coda piena
    if (pool->count >= queue_size)
    {

        pthread_cond_wait(&(pool->cond_coda), &(pool->lock));

        // UNLOCK_RETURN(&(pool->lock), -1);
        // return 1; // esco con valore "coda piena"
    }
    // coda in fase di uscita
    if (pool->exiting)
    {
        UNLOCK_RETURN(&(pool->lock), -1);
        return 1; // esco con valore "coda in fase di uscita"
    }

    if (pool->taskonthefly >= pool->numthreads)
    {
        if (nopending)
        {
            // tutti i thread sono occupati e non si gestiscono task pendenti
            assert(pool->count == 0);

            UNLOCK_RETURN(&(pool->lock), -1);
            return 1; // esco con valore "coda piena"
        }
    }

    pool->pending_queue[pool->tail].fun = f;
    pool->pending_queue[pool->tail].arg = arg;
    pool->count++;
    pool->tail++;
    if (pool->tail >= queue_size)
        pool->tail = 0;

    int r;
    if ((r = pthread_cond_signal(&(pool->cond))) != 0)
    {
        UNLOCK_RETURN(&(pool->lock), -1);
        errno = r;
        return -1;
    }

    UNLOCK_RETURN(&(pool->lock), -1);
    return 0;
}

/**
 * @function void *thread_proxy(void *argl)
 * @brief funzione eseguita dal thread worker che non appartiene al pool
 */
static void *proxy_thread(void *arg)
{
    taskfun_t *task = (taskfun_t *)arg;
    // eseguo la funzione
    (*(task->fun))(task->arg);

    free(task);
    return NULL;
}

// fa lo spawn di un thread in modalità detached
int spawnThread(void (*f)(void *), void *arg)
{
    if (f == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    taskfun_t *task = malloc(sizeof(taskfun_t)); // la memoria verra' liberata dal proxy
    if (!task)
        return -1;
    task->fun = f;
    task->arg = arg;

    pthread_t thread;
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0)
        return -1;
    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0)
        return -1;
    if (pthread_create(&thread, &attr, proxy_thread, (void *)task) != 0)
    {
        free(task);
        errno = EFAULT;
        return -1;
    }
    return 0;
}
