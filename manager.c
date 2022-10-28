#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "carpark_details.h"
#include "hashtable.h"
#include "shared_memory.h"
#include "capacity.h"
#include "utility.h"
#include "manager_routines.h"

#define SHM_NAME "/PARKING"
#define SHM_SZ sizeof(data_t)

#define SHM_EST_SEM_NAME "/SHM_ESTABLISHED"
#define SIM_READY_SEM_NAME "/SIM_READY"
#define SIM_END_SEM_NAME "/SIM_ENDED"
#define MAN_READY_SEM_NAME "/MAN_READY"
#define MAN_END_SEM_NAME "/MAN_ENDED"

#define PLATE_FILE "plates.txt"
#define MAX_LINE_LENGTH 7
#define MAX_IMPORTED_PLATES 100

static pthread_mutex_t initialisation_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t initialisation_cond = PTHREAD_COND_INITIALIZER;

static htab_t hashtable;
static capacity_t capacity;

int main()
{
    shared_memory_t shm;

    sem_t *shm_established_sem = sem_open(SHM_EST_SEM_NAME, 0);
    sem_t *simulation_ready_sem = sem_open(SIM_READY_SEM_NAME, 0);
    sem_t *manager_ready_sem = sem_open(MAN_READY_SEM_NAME, 0);
    // simulation_ended_sem = sem_open(SIM_END_SEM_NAME, 0);
    // manager_ended_sem = sem_open(MAN_END_SEM_NAME, 0);

    pthread_t monitor_sim_thread;

    // Initialise hash table from "plates.txt" file
    hashtable = import_htable(PLATE_FILE);

    // Initialise Capcity
    init_capacity(&capacity);
    // print_capacity(&capacity);

    printf("Manager started.\n");
    if (get_shared_object(&shm))
    {
        sem_post(shm_established_sem);
        printf("Manager successfully connected to shm.");
    }
    else
    {
        perror("Shared memory connection failed.");
        return -1;
    }

    printf("Init Entrance threads.\n");
    pthread_t entrance_BG_threads[ENTRANCES];
    pthread_t entrance_threads[ENTRANCES];
    entrance_t *entrance;
    entrance_data_t entrance_datas[ENTRANCES];
    for (size_t i = 0; i < ENTRANCES; i++)
    {
        get_entrance(&shm, i, &entrance);
        entrance_datas[i].entrance = entrance;
        entrance_datas[i].capacity = &capacity;
        entrance_datas[i].hashtable = &hashtable;
        boom_gate_t *boom_gate = malloc(sizeof(boom_gate_t));
        boom_gate = &entrance->boom_gate;
        printf("Entrance %ld Boom Gate %p\n", i, &entrance->boom_gate);
        pthread_create(&entrance_threads[i], NULL, monitor_entrance, (void *)&entrance_datas[i]);
    }

    printf("Waiting for simulation to ready.\n");
    sem_wait(simulation_ready_sem);
    printf("=================.\n");

    printf("Manager ready for simulation.\n");
    sem_post(manager_ready_sem);
    printf("=================.\n");
    printf("Start Monitor Thread.\n");
    pthread_create(&monitor_sim_thread, NULL, wait_sim_close, (void *)NULL);

    pthread_join(monitor_sim_thread, NULL);
    printf("=================.\n");
    printf("Joined Monitor Thread.\n");

    printf("=================.\n");
    printf("Joined All Thread.\n");

    printf("Manager Exited.\n");

    for (size_t i = 0; i < ENTRANCES; i++)
    {
        get_entrance(&shm, i, &entrance);
        boom_gate_t *boom_gate = malloc(sizeof(boom_gate_t));
        boom_gate = &entrance->boom_gate;
        printf("Final Boomgate %p status: %c\n", boom_gate, boom_gate->status);
    }
    return 0;
}
