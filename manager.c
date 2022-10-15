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
#include <semaphore.h>

#include "carpark_types.h"
#include "shared_memory.h"

#define SHM_NAME "/PARKING"
#define SHM_SZ sizeof(data_t)

#define SHM_EST_SEM_NAME "/SHM_ESTABLISHED"
#define SIM_END_SEM_NAME "/SIM_ENDED"


int main() {
    shared_memory_t shm;
    // sem_t *shm_established_sem = sem_open(SHM_EST_SEM_NAME, 0);
    sem_t *simulation_ended_sem = sem_open(SIM_END_SEM_NAME, O_CREAT, 0644, 0);

    printf("Manager started.\n");
    if (get_shared_object(&shm)) {
        printf("Manager has read it and put in test value\n");
        // sem_post(shm_established_sem);
    } else {
        printf("Shared memory connection failed.\n");
        return -1;
    }
    sem_wait(simulation_ended_sem);

    // printf("%ld\n", sizeof(data_t));
    printf("Manager finished.\n");
    return 0;
}
