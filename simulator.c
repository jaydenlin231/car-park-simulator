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
#define SIM_READY_SEM_NAME "/SIM_READY"
#define SIM_END_SEM_NAME "/SIM_ENDED"
#define MAN_READY_SEM_NAME "/MAN_READY"
#define MAN_END_SEM_NAME "/MAN_ENDED"


// TODO: Remove testing stub
#define NUM_OF_CARS 5

pthread_mutex_t initialisation_mutex;
pthread_cond_t initialisation_cond;

sem_t *manager_ended_sem;

// TODO: Remove testing stub
int ready_threads = 0;

// TODO: Remove testing stub
void *test_thread(void *data) {
    pthread_mutex_lock(&initialisation_mutex);
    int *test = (int*) data;
    printf("Thread %d Received data: %d.\n", *test, *test);
    free(test);
    ready_threads++;
    pthread_cond_signal(&initialisation_cond);
    pthread_mutex_unlock(&initialisation_mutex);
}

void *wait_manager_close(void *data)
{
    printf("Manager monitor thread waiting.\n");
    sem_wait(manager_ended_sem);
    printf("Manager monitor thread Woke\n");
}

void *control_boom_gate(boom_gate_t *boom_gate, char update_status) {
    if(update_status == BG_RAISING) {
            pthread_mutex_lock(&boom_gate->mutex);
            printf("Instruct Boom Gate %p Raising\n", boom_gate);
            boom_gate->status = BG_RAISING;
            pthread_mutex_unlock(&boom_gate->mutex);
            pthread_cond_broadcast(&boom_gate->cond);

        } else if(update_status == BG_LOWERING) {
            printf("Instruct Boom Gate Lowering\n");
            pthread_mutex_lock(&boom_gate->mutex);
            boom_gate->status = BG_LOWERING;
            pthread_mutex_unlock(&boom_gate->mutex);
            pthread_cond_broadcast(&boom_gate->cond);
        }
}


void *handle_boom_gate(void *data) {
    boom_gate_t *boom_gate = (boom_gate_t *)data;

    printf("Created Boom Gate %p Thread\n", boom_gate);

    pthread_mutex_lock(&initialisation_mutex);
    ready_threads++;
    pthread_cond_broadcast(&initialisation_cond);
    pthread_mutex_unlock(&initialisation_mutex);
    int num = 0;
    int test = 0;
    while (true) {
        printf("\tBoom Gate %p Monitor Loop ran %d times.\n",boom_gate, ++test);

        pthread_mutex_lock(&boom_gate->mutex);
        
        while (!(boom_gate->status == BG_RAISING || boom_gate->status == BG_LOWERING)) {
            printf("\tBoom Gate %p Cond Wait BG_RAISING or BG_LOWERING. Current Status: %c.\n", boom_gate, boom_gate->status);
            pthread_cond_wait(&boom_gate->cond, &boom_gate->mutex);
        }
        pthread_mutex_unlock(&boom_gate->mutex);
        
        printf("Boom Gate %p Received Instruction Status: %c.\n", boom_gate, boom_gate->status);
        if(boom_gate->status == BG_RAISING) {
            printf("Raising Boom Gate %p...\n", boom_gate);
            printf("Waiting 10 ms\n");
            sleep(2);
            pthread_mutex_lock(&boom_gate->mutex);
            boom_gate->status = BG_OPENED;
            printf("Boom Gate %p Opened\n", boom_gate);
            pthread_cond_signal(&boom_gate->cond);
            pthread_mutex_unlock(&boom_gate->mutex);
        } else if(boom_gate->status == BG_LOWERING) {
            printf("Lowering Boom Gate %p...\n", boom_gate);
            printf("Waiting 10 ms\n");
            sleep(2);
            pthread_mutex_lock(&boom_gate->mutex);
            boom_gate->status = BG_CLOSED;
            printf("Boom Gate %p Closed\n", boom_gate);
            pthread_cond_signal(&boom_gate->cond);
            pthread_mutex_unlock(&boom_gate->mutex);
        } 
    }
    printf("Boom Gate Quit before broadcast %p...\n", boom_gate);
    return NULL;
}

// TODO: Remove testing stub
void *generate_cars(void *arg) {
    for (size_t i = 0; i < NUM_OF_CARS; i++) {
        printf("Generating Car %i ...\n", i);
        sleep(2);
    }
}


int main() {
    shared_memory_t shm;
    if(sem_unlink(SHM_EST_SEM_NAME) !=0){
        perror("sem_unlink(SHM_EST_SEM_NAME) failed");
    }
    if(sem_unlink(SIM_READY_SEM_NAME) != 0){
        perror("shm_unlink(SIM_READY_SEM_NAME) failed");
    }
    if(sem_unlink(SIM_END_SEM_NAME) != 0){
        perror("shm_unlink(SIM_END_SEM_NAME) failed");
    }
    if(sem_unlink(MAN_READY_SEM_NAME) != 0){
        perror("shm_unlink(SIM_END_SEM_NAME) failed");
    }
    if(sem_unlink(MAN_END_SEM_NAME) != 0){
        perror("shm_unlink(SIM_END_SEM_NAME) failed");
    }
    
    
    sem_t *shm_established_sem = sem_open(SHM_EST_SEM_NAME, O_CREAT, 0644, 0);
    sem_t *simulation_ready_sem = sem_open(SIM_READY_SEM_NAME, O_CREAT, 0644, 0);
    sem_t *simulation_ended_sem = sem_open(SIM_END_SEM_NAME, O_CREAT, 0644, 0);
    sem_t *manager_ready_sem = sem_open(MAN_READY_SEM_NAME, O_CREAT, 0644, 0);
    manager_ended_sem = sem_open(MAN_END_SEM_NAME, O_CREAT, 0644, 0);

    pthread_mutex_init(&initialisation_mutex, NULL);
    pthread_cond_init(&initialisation_cond, NULL);
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

    printf("Waiting for Manager to connect to shm.\n");
    sem_wait(shm_established_sem);
    printf("Manager connected to shm.\n");

    init_entrance_data(&shm);
    printf("Finished init entrance data\n");

    pthread_t car_generation_thread;
    pthread_t monitor_sim_thread;

    pthread_t entrance_threads[ENTRANCES];
    pthread_t exit_threads[EXITS];
    pthread_t level_threads[LEVELS];
    
    printf("Init Entrance threads.\n");

    entrance_t *entrance = malloc(sizeof(entrance_t));
    for (int i = 0; i < ENTRANCES; i++) {
        get_entrance(&shm, i, &entrance);
        boom_gate_t* boom_gate = malloc(sizeof(boom_gate_t));
        boom_gate = &entrance->boom_gate;
        pthread_create(&entrance_threads[i], NULL, handle_boom_gate, (void *)boom_gate);
    }
    // // exit_t *exit;
    // for (int i = 0; i < EXITS; i++) {
    //     int* data = malloc(sizeof(int));
    //     *data = i;
    //     pthread_create(&exit_threads[i], NULL, test_thread, (void *)data);
    // }
    // // level_t *level;
    // for (int i = 0; i < LEVELS; i++) {
    //     int* data = malloc(sizeof(int));
    //     *data = i;
    //     pthread_create(&level_threads[i], NULL, test_thread, (void *)data);
    // }
    

    printf("=================.\n");
    printf("Start Monitor Thread.\n");
    pthread_create(&monitor_sim_thread, NULL, wait_manager_close, NULL);
    
    printf("Checking Init Entrance threads.\n");
    pthread_mutex_lock(&initialisation_mutex);
    int num = 0;
    // TODO: Add number of thread def?
    while(ready_threads < (ENTRANCES)){
        pthread_cond_wait(&initialisation_cond, &initialisation_mutex);
        num ++;
        printf("Checking condition for %d times.\n", num);
        printf("Ready %d threads.\n", ready_threads);
    }
    pthread_mutex_unlock(&initialisation_mutex);
    printf("Finish Init Entrance threads.\n");
    
    sem_post(simulation_ready_sem);
    printf("Simulation ready to start\n");
    printf("Waiting for manager ready\n");

    sem_wait(manager_ready_sem);

    for (int i = 0; i < ENTRANCES; i++) {
        if(i % 2 == 1){
            printf("Instructing Entrance %d\n", i);
            get_entrance(&shm, i, &entrance);
            boom_gate_t* boom_gate = malloc(sizeof(boom_gate_t));
            boom_gate = &entrance->boom_gate;
            control_boom_gate(boom_gate, BG_RAISING);
            sleep(2);
        }
    }
    
    printf("=================.\n");
    printf("Start Car Thread.\n");
    pthread_create(&car_generation_thread, NULL, generate_cars, NULL);

    pthread_join(car_generation_thread, NULL);
    printf("=================.\n");
    printf("Joined Car Thread.\n");
    
    sem_post(simulation_ended_sem);
    
    pthread_join(monitor_sim_thread, NULL);
    printf("=================.\n");
    printf("Manager ended.\n");

    // Might need this?
    // clean_shared_memory_data(&shm);
    // printf("clean_shared_memory_data done.\n");
    destroy_shared_object(&shm);
    printf("Destroyed shm.\n");

    printf("=================.\n");
    printf("Simulator Exited.\n");
    return 0;
}