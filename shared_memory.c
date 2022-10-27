#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "carpark_details.h"
#include "shared_memory.h"

#define SHM_NAME "/PARKING"
#define SHM_SZ sizeof(data_t)

bool create_shared_object(shared_memory_t *shm)
{
    // Remove any previous instance of the shared memory object, if it exists.
    if (shm_unlink(SHM_NAME) != 0)
    {
        perror("shm_unlink() failed");
    }

    // Assign share name to shm->name.
    shm->name = SHM_NAME;

    // Create the shared memory object, allowing read-write access, and saving the
    // resulting file descriptor in shm->fd.
    if ((shm->fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666)) < 0)
    {
        perror("shm_open error");
        shm->data = NULL;
        return false;
    }

    // Set the capacity of the shared memory object via ftruncate. If the
    // operation fails.
    if (ftruncate(shm->fd, SHM_SZ))
    {
        perror("ftruncate error");
        shm->data = NULL;
        return false;
    }
    // Attempt to map the shared memory via mmap, and save the address
    // in shm->data.
    if ((shm->data = mmap(0, SHM_SZ, PROT_WRITE, MAP_SHARED, shm->fd, 0)) == (data_t *)-1)
    {
        perror("mmap error");
        return false;
    }

    return true;
}

void destroy_shared_object(shared_memory_t *shm)
{
    // Remove the shared memory object.
    if (munmap(shm->data, SHM_SZ) != 0)
    {
        perror("munmap() failed");
    }
    if (shm_unlink(shm->name) != 0)
    {
        perror("shm_unlink() failed");
    }
    shm->fd = -1;
    shm->data = NULL;
}

bool get_shared_object(shared_memory_t *shm)
{
    // Get a file descriptor connected to shared memory object and save in
    // shm->fd.
    if ((shm->fd = shm_open(SHM_NAME, O_RDWR, 0)) < 0)
    {
        perror("shm_open");
        shm->data = NULL;
        return false;
    }
    // Otherwise, attempt to map the shared memory via mmap, and save the address
    // in shm->data.
    if ((shm->data = mmap(0, SHM_SZ, PROT_READ | PROT_WRITE, MAP_SHARED, shm->fd, 0)) == (void *)-1)
    {
        perror("mmap");
        return false;
    }

    return true;
}

void showPshared(pthread_mutexattr_t *mta)
{
    //   int           rc;
    int pshared;

    printf("Check pshared attribute\n");
    pthread_mutexattr_getpshared(mta, &pshared);

    printf("The pshared attributed is: ");
    switch (pshared)
    {
    case PTHREAD_PROCESS_PRIVATE:
        printf("PTHREAD_PROCESS_PRIVATE\n");
        break;
    case PTHREAD_PROCESS_SHARED:
        printf("PTHREAD_PROCESS_SHARED\n");
        break;
    default:
        printf("! pshared Error !\n");
        exit(1);
    }
    return;
}

void init_entrance_data(shared_memory_t *shm)
{

    entrance_t *entrance;
    for (size_t i = 0; i < ENTRANCES; i++)
    {
        get_entrance(shm, i, &entrance);
        printf("Entrance %ld address %p\n", i, entrance);
        pthread_mutex_lock(&entrance->boom_gate.mutex);
        entrance->boom_gate.status = BG_CLOSED;
        pthread_cond_broadcast(&entrance->boom_gate.cond);
        pthread_mutex_unlock(&entrance->boom_gate.mutex);
        // TODO: init Info sign here
        printf("Entrance %ld Boom Gate %p status %c\n", i, &entrance->boom_gate, entrance->boom_gate.status);
    }
}

void init_shared_memory_data(shared_memory_t *shm)
{
    // Set process shared attributes
    pthread_mutexattr_t mutexattr;
    pthread_condattr_t condattr;

    if (pthread_mutexattr_init(&mutexattr) != 0)
    {
        perror("pthread_mutexattr_setpshared failed");
        exit(1);
    }
    if (pthread_condattr_init(&condattr) != 0)
    {
        perror("pthread_condattr_setpshared failed");
        exit(1);
    }
    if (pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED) != 0)
    {
        perror("pthread_mutexattr_setpshared failed");
        exit(1);
    }
    if (pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_ERRORCHECK_NP) != 0)
    {
        perror("pthread_mutexattr_setpshared failed");
        exit(1);
    }
    if (pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED) != 0)
    {
        perror("pthread_condattr_setpshared failed");
        exit(1);
    }

    showPshared(&mutexattr);

    entrance_t *entrance;
    for (size_t i = 0; i < ENTRANCES; i++)
    {
        get_entrance(shm, i, &entrance);
        if (pthread_mutex_init(&(entrance->lpr.mutex), &mutexattr) != 0)
        {
            perror("entrance lpr pthread mutex attr init");
            exit(1);
        }
        if (pthread_cond_init(&entrance->lpr.cond, &condattr) != 0)
        {
            perror("entrance lpr pthread cond attr init");
            exit(1);
        }
        if (pthread_mutex_init(&entrance->boom_gate.mutex, &mutexattr) != 0)
        {
            perror("entrance boom gate pthread mutex attr init");
            exit(1);
        }
        if (pthread_cond_init(&entrance->boom_gate.cond, &condattr) != 0)
        {
            perror("entrance boom gate pthread cond attr init");
            exit(1);
        }
        if (pthread_mutex_init(&entrance->info_sign.mutex, &mutexattr) != 0)
        {
            perror("entrance info sign pthread mutex attr init");
            exit(1);
        }
        if (pthread_cond_init(&entrance->info_sign.cond, &condattr) != 0)
        {
            perror("entrance info sign pthread cond attr init");
            exit(1);
        }
        printf("Entrance %ld: %p\n", i, entrance);
        // TODO: init LRP data here
        // TODO: init Booom gate here
        // pthread_mutex_lock(&entrance->boom_gate.mutex);
        // entrance->boom_gate.status = BG_CLOSED;
        // pthread_cond_signal(&entrance->boom_gate.cond);
        // pthread_mutex_unlock(&entrance->boom_gate.mutex);
        // // TODO: init Info sign here
        // printf("Entrance %d Boom Gate %p status %c\n",i, &entrance->boom_gate,entrance->boom_gate.status);
    }

    exit_t *exit;
    for (size_t i = 0; i < EXITS; i++)
    {
        get_exit(shm, i, &exit);
        if (pthread_mutex_init(&exit->lpr.mutex, &mutexattr) != 0)
        {
            perror("exit lpr pthread mutex attr init");
            printf("%d\n", pthread_mutex_init(&exit->lpr.mutex, &mutexattr));
        }
        pthread_cond_init(&exit->lpr.cond, &condattr);
        pthread_mutex_init(&exit->boom_gate.mutex, &mutexattr);
        pthread_cond_init(&exit->boom_gate.cond, &condattr);
        printf("Exit %ld: %p\n", i, exit);
        // TODO: init LRP data here
        // TODO: init Booom gate here
        exit->boom_gate.status = BG_CLOSED;
    }

    level_t *level;
    for (size_t i = 0; i < LEVELS; i++)
    {
        get_level(shm, i, &level);
        pthread_mutex_init(&level->lpr.mutex, &mutexattr);
        pthread_cond_init(&level->lpr.cond, &condattr);
        printf("Level %ld: %p\n", i, level);
        // TODO: init LRP data here
    }

    // pthread_mutexattr_destroy(&mutexattr);
    // pthread_condattr_destroy(&condattr);
    return;
}

void clean_shared_memory_data(shared_memory_t *shm)
{
    entrance_t *entrance;
    for (size_t i = 0; i < ENTRANCES; i++)
    {
        get_entrance(shm, i, &entrance);
        pthread_mutex_destroy(&entrance->lpr.mutex);
        pthread_cond_destroy(&entrance->lpr.cond);
        pthread_mutex_destroy(&entrance->boom_gate.mutex);
        pthread_cond_destroy(&entrance->boom_gate.cond);
        pthread_mutex_destroy(&entrance->info_sign.mutex);
        pthread_cond_destroy(&entrance->info_sign.cond);
    }

    exit_t *exit;
    for (size_t i = 0; i < EXITS; i++)
    {
        get_exit(shm, i, &exit);
        pthread_mutex_destroy(&exit->lpr.mutex);
        pthread_cond_destroy(&exit->lpr.cond);
        pthread_mutex_destroy(&exit->boom_gate.mutex);
        pthread_cond_destroy(&exit->boom_gate.cond);
    }

    level_t *level;
    for (size_t i = 0; i < LEVELS; i++)
    {
        get_level(shm, i, &level);
        pthread_mutex_destroy(&level->lpr.mutex);
        pthread_cond_destroy(&level->lpr.cond);
    }
    return;
}

// Refer to following.
// https://stackoverflow.com/questions/40167559/in-c-how-would-i-choose-whether-to-return-a-struct-or-a-pointer-to-a-struct
void get_entrance(shared_memory_t *shm, int i, entrance_t **entrance)
{
    // TODO: error handling and return status code/bool?
    *entrance = &(shm->data->entrances[i]);
    return;
}
void get_exit(shared_memory_t *shm, int i, exit_t **exit)
{
    // TODO: error handling and return status code/bool?
    *exit = &(shm->data->exits[i]);
    return;
}
void get_level(shared_memory_t *shm, int i, level_t **level)
{
    // TODO: error handling and return status code/bool?
    *level = &(shm->data->levels[i]);
    return;
}