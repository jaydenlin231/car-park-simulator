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

// TODO: Remove testing stub
#define NUM_OF_CARS 10

static pthread_mutex_t initialisation_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t initialisatoin_cond = PTHREAD_COND_INITIALIZER;

// TODO: Remove testing stub
int ready_threads = 0;

// TODO: Remove testing stub
void *test_thread(void *data) {
    pthread_mutex_lock(&initialisation_mutex);
    int *test = (int*) data;
    printf("Entrance Thread %d. Waiting...\n", *test);
    printf("Entrance Thread %d Received data: %d.\n", *test, *test);
    free(test);
    ready_threads++;
    pthread_cond_signal(&initialisatoin_cond);
    pthread_mutex_unlock(&initialisation_mutex);

}

// TODO: Remove testing stub
void *generate_cars(void *arg) {
    for (size_t i = 0; i < NUM_OF_CARS; i++) {
        printf("Generating Car %i ...\n", i);
        sleep(1);
    }
}

int main() {
    shared_memory_t shm;
    sem_t *shm_established_sem = sem_open(SHM_EST_SEM_NAME, O_CREAT, 0644, 0);
    sem_t *simulation_ended_sem = sem_open(SIM_END_SEM_NAME, 0);

    printf("Attempt to create shared memory.\n");
    if (create_shared_object(&shm)) {
        printf("Created shared memory.\n");
        printf("Init shared data.\n");
        init_shared_memory_data(&shm);
        printf("Finished init shared data.\n");
    } else {
        printf("Shared memory creation failed.\n");
        return -1;
    }

    printf("Waiting for Manager.\n");
    // sem_wait(shm_established_sem);

    printf("Manager connected.\n");
    printf("=================.\n");

    pthread_t car_generation_thread;
    pthread_t entrance_threads[ENTRANCES];
    pthread_t exit_threads[EXITS];
    pthread_t level_threads[LEVELS];
    
    printf("Init Entrance threads.\n");
    for (size_t i = 0; i < ENTRANCES; i++) {
        int* data = malloc(sizeof(int));
        // TODO: Remove testing stub
        *data = i;
        pthread_create(&entrance_threads[i], NULL, test_thread, (void *)data);
    }
    
    pthread_mutex_lock(&initialisation_mutex);
    int num = 0;
    while(ready_threads < ENTRANCES){
        pthread_cond_wait(&initialisatoin_cond, &initialisation_mutex);
        num ++;
        printf("Checking condition for %d times.\n", num);
        printf("Ready %d threads.\n", ready_threads);
    }
    pthread_mutex_unlock(&initialisation_mutex);

    printf("Finish Init Entrance threads.\n");
    
    printf("=================.\n");
    printf("Start Car Thread.\n");
    pthread_create(&car_generation_thread, NULL, generate_cars, NULL);

    pthread_join(car_generation_thread, NULL);
    printf("=================.\n");
    printf("Joined Car Thread.\n");

    for (int i = 0; i < ENTRANCES; i++) {
        pthread_join(entrance_threads[i], NULL);
    }
    printf("=================.\n");
    printf("Joined Entrance Thread.\n");

    sem_post(simulation_ended_sem);
    destroy_shared_object(&shm);

    printf("=================.\n");
    printf("Simulator finished.\n");
    return 0;
}