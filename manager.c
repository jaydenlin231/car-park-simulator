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
#include <errno.h>

#include "carpark_types.h"
#include "shared_memory.h"

#define SHM_NAME "/PARKING"
#define SHM_SZ sizeof(data_t)

#define SHM_EST_SEM_NAME "/SHM_ESTABLISHED"
#define SIM_READY_SEM_NAME "/SIM_READY"
#define SIM_END_SEM_NAME "/SIM_ENDED"
#define MAN_END_SEM_NAME "/MAN_ENDED"

static pthread_mutex_t initialisation_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t initialisation_cond = PTHREAD_COND_INITIALIZER;
// TODO: Remove testing stub
int ready_threads = 0;

sem_t *simulation_ready_sem;
sem_t *simulation_ended_sem;
sem_t *quit_manager_sem;


static pthread_mutex_t quit_mutex = PTHREAD_MUTEX_INITIALIZER;
bool quit = false;

void *handle_boom_gate(void *data) {
    boom_gate_t *boom_gate = (boom_gate_t *)data;

    printf("Created Boom Gate %p Thread\n", boom_gate);
    printf("Boom Gate %p Thread Waiting...\n", boom_gate);

    pthread_mutex_lock(&initialisation_mutex);
    ready_threads++;
    pthread_mutex_unlock(&initialisation_mutex);
    pthread_cond_signal(&initialisation_cond);

    printf("Begin Boom Gate %p Thread\n", boom_gate);
    while (true) {
        printf("\tBoom Gate %p Before lock \n", boom_gate);
        if(pthread_mutex_lock(&boom_gate->mutex) != 0){
            perror("pthread_mutex_lock");
            exit(1);
        }
        printf("\tBoom Gate %p After lock \n", boom_gate);
        
        while (boom_gate->status != BG_OPENED && boom_gate->status != BG_QUIT) {
            printf("\tBoom Gate %p Cond wait \n", boom_gate);
            if(pthread_cond_wait(&boom_gate->cond, &boom_gate->mutex) != 0){
                printf("\t\tBoom Gate %p Cond Wait Error\n", boom_gate);
                perror("\t\tBoom Gate pthread_cond_wait");
                printf("\t\tBoom Gate %p Stauts: %c \n", boom_gate, boom_gate->status);
                printf("\t\tBoom Gate %p Cond wait Mutex addr:%p Cond addr:%p\n", boom_gate, &boom_gate->mutex, &boom_gate->cond);
                printf("\t\t%d\n", pthread_cond_wait(&boom_gate->cond, &boom_gate->mutex));
                exit(1);
            }
        }
        printf("\tBoom Gate %p Before Unlock \n", boom_gate);
        if(pthread_mutex_unlock(&boom_gate->mutex) != 0){
            perror("pthread_mutex_lock");
            exit(1);
        }
        printf("\tBoom Gate %p After Unlock \n", boom_gate);

        if(boom_gate->status == BG_QUIT){
            printf("Boom Gate Quit from waiting %p...\n", boom_gate);
            return NULL;
        }

        printf("Closing Boom Gate %p...\n", boom_gate);
        printf("Waiting 10 ms\n");
        sleep(2);
        pthread_mutex_lock(&boom_gate->mutex);
        boom_gate->status = BG_LOWERING;
        pthread_cond_signal(&boom_gate->cond);
        printf("Boom Gate %p Lowered\n", boom_gate);
        pthread_mutex_unlock(&boom_gate->mutex);
        printf("Boom Gate %p After unlock \n", boom_gate);
    }
    printf("Boom Gate Quit before broadcast %p...\n", boom_gate);
    return NULL;
    
}

void *wait_sim_close(void *data)
{
    shared_memory_t *shm = (shared_memory_t *)data;
    printf("Sim Thread Waiting\n");
    sem_wait(simulation_ended_sem);
    printf("Sim Thread Woke\n");
    pthread_mutex_lock(&quit_mutex);
    quit = true;
    pthread_mutex_unlock(&quit_mutex);
    // entrance_t *entrance;
    // for (size_t i = 0; i < ENTRANCES; i++) {
    //     get_entrance(shm, i, &entrance);
    //     // pthread_mutex_lock(&entrance->boom_gate.mutex);
    //     printf("Set Boom Gate %p to Quit...\n", &entrance->boom_gate);
    //     entrance->boom_gate.status = BG_QUIT;
    //     pthread_cond_signal(&entrance->boom_gate.cond);
    //     // pthread_mutex_unlock(&entrance->boom_gate.mutex);
    // }
}

int main() {
    shared_memory_t shm;
    sem_t *shm_established_sem = sem_open(SHM_EST_SEM_NAME, 0);
    simulation_ready_sem = sem_open(SIM_READY_SEM_NAME, 0);
    simulation_ended_sem = sem_open(SIM_END_SEM_NAME, 0);
    sem_t *manager_ended_sem = sem_open(MAN_END_SEM_NAME, O_CREAT, 0644, 0);
    pthread_t monitor_sim_thread;

    printf("Manager started.\n");
    if (get_shared_object(&shm)) {
        printf("Manager has read it and put in test value\n");
        // sem_post(shm_established_sem);
        printf("Waiting simulation ready.\n");
        // sem_wait(simulation_ready_sem);
    } else {
        perror("Shared memory connection failed.");
        return -1;
    }

    
    pthread_t entrance_threads[ENTRANCES];

    printf("Init Entrance threads.\n");

    int num = 0;
    entrance_t *entrance;
    for (size_t i = 0; i < ENTRANCES; i++) {
        get_entrance(&shm, i, &entrance);
        boom_gate_t* boom_gate = malloc(sizeof(boom_gate_t));
        boom_gate = &entrance->boom_gate;
        printf("Entrance %d Boom Gate %p\n",i, &entrance->boom_gate);
        pthread_create(&entrance_threads[i], NULL, handle_boom_gate, (void *)boom_gate);
    }
   
    printf("=================.\n");
    printf("Start Monitor Thread.\n");
    pthread_create(&monitor_sim_thread, NULL, wait_sim_close, (void*)&shm);

    pthread_mutex_lock(&initialisation_mutex);
    while(ready_threads < (ENTRANCES)){
        printf("Waiting Ready Thread Condition for %d times.\n", ++num);
        pthread_cond_wait(&initialisation_cond, &initialisation_mutex);
        printf("Checking Thread Condition for %d times.\n", num);
        printf("Ready %d threads.\n", ready_threads);
    }
    pthread_mutex_unlock(&initialisation_mutex);
        sem_post(shm_established_sem);
        sem_wait(simulation_ready_sem);

    pthread_join(monitor_sim_thread, NULL);
    printf("=================.\n");
    printf("Joined Monitor Thread.\n");
    // for (int i = 0; i < ENTRANCES; i++) {
    //     get_entrance(&shm, i, &entrance);
    //     // pthread_mutex_lock(&entrance->boom_gate.mutex);
    //     printf("Set Boom Gate %p to Quit...\n", &entrance->boom_gate);
    //     entrance->boom_gate.status = BG_QUIT;
    //     pthread_cond_signal(&entrance->boom_gate.cond);
    //     pthread_join(entrance_threads[i], NULL);
    // }
    // printf("Quit.\n");
    // quit = true;
    
    printf("=================.\n");
    printf("Joined All Thread.\n");
    sem_post(manager_ended_sem);

    // printf("%ld\n", sizeof(data_t));
    printf("Manager finished.\n");
    return 0;
}
