#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "carpark_types.h"
#include "hashtable.h"
#include "shared_memory.h"

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
// TODO: Remove testing stub
int ready_threads = 0;

sem_t *simulation_ended_sem;
sem_t *manager_ended_sem;

void *handle_boom_gate(void *data) {
    boom_gate_t *boom_gate = (boom_gate_t *)data;

    printf("Created Boom Gate %p Thread\n", boom_gate);

    pthread_mutex_lock(&initialisation_mutex);
    ready_threads++;
    pthread_mutex_unlock(&initialisation_mutex);
    pthread_cond_signal(&initialisation_cond);

    printf("Begin Boom Gate %p Thread\n", boom_gate);
    int test = 0;
    while (true) {
        printf("\tBoom Gate %p Monitor Loop ran %d times.\n", boom_gate, ++test);

        pthread_mutex_lock(&boom_gate->mutex);
        while (!(boom_gate->status == BG_OPENED)) {
            printf("\tBoom Gate %p Cond Wait BG_OPENED. Current Status: %c.\n", boom_gate, boom_gate->status);
            pthread_cond_wait(&boom_gate->cond, &boom_gate->mutex);
        }
        pthread_mutex_unlock(&boom_gate->mutex);

        if (boom_gate->status == BG_OPENED) {
            printf("Lowering Boom Gate %p...\n", boom_gate);
            printf("Waiting 10 ms\n");
            sleep(2);
            pthread_mutex_lock(&boom_gate->mutex);
            boom_gate->status = BG_LOWERING;
            pthread_cond_broadcast(&boom_gate->cond);
            printf("Boom Gate %p Lowered\n", boom_gate);
            pthread_mutex_unlock(&boom_gate->mutex);
        } else if (boom_gate->status == BG_CLOSED) {
            printf("Lowering Boom Gate %p...\n", boom_gate);
            printf("Waiting 10 ms\n");
            sleep(2);
            pthread_mutex_lock(&boom_gate->mutex);
            boom_gate->status = BG_RAISING;
            pthread_cond_broadcast(&boom_gate->cond);
            printf("Boom Gate %p Lowered\n", boom_gate);
            pthread_mutex_unlock(&boom_gate->mutex);
        }
    }
    // Might not be needed
    printf("Boom Gate Quit before broadcast %p...\n", boom_gate);
    return NULL;
}

void *wait_sim_close(void *data) {
    shared_memory_t *shm = (shared_memory_t *)data;
    printf("Monitor Thread Waiting for simulation to close\n");
    sem_wait(simulation_ended_sem);
    printf("Monitor notified simulation closed\n");

    sem_post(manager_ended_sem);
}

htab_t import_htable(char fname[]) {
    htab_t htable;
    FILE *textfile;
    char line[MAX_LINE_LENGTH];
    int plates_quantity = 0;

    if (!htab_init(&htable, MAX_IMPORTED_PLATES)) {
        printf("Error initialising htable\n");
    }

    textfile = fopen(fname, "r");
    if (textfile == NULL) {
        printf("Error reading plates file");
    }

    while (plates_quantity < MAX_IMPORTED_PLATES && fscanf(textfile, "%s", line) != EOF) {
        // printf("Read line %s.\n", line);
        char *plate_copy = malloc(sizeof(char) * (strlen(line) + 1));
        strcpy(plate_copy, line);
        plates_quantity++;
        if (htab_find(&htable, plate_copy) != NULL) {
            printf("Duplicated item in htable\n");
            continue;
        }
        if (!htab_add(&htable, plate_copy, plates_quantity)) {
            printf("Failed to add item into htable\n");
        }
    }
    fclose(textfile);
    return htable;
}

int main() {
    shared_memory_t shm;

    sem_t *shm_established_sem = sem_open(SHM_EST_SEM_NAME, 0);
    sem_t *simulation_ready_sem = sem_open(SIM_READY_SEM_NAME, 0);
    sem_t *manager_ready_sem = sem_open(MAN_READY_SEM_NAME, 0);
    simulation_ended_sem = sem_open(SIM_END_SEM_NAME, 0);
    manager_ended_sem = sem_open(MAN_END_SEM_NAME, 0);

    pthread_t monitor_sim_thread;

    // Initialise hash table from "plates.txt" file
    htab_t hashtable = import_htable(PLATE_FILE);

    printf("Manager started.\n");
    if (get_shared_object(&shm)) {
        sem_post(shm_established_sem);
        printf("Manager successfully connected to shm.");
    } else {
        perror("Shared memory connection failed.");
        return -1;
    }

    printf("Init Entrance threads.\n");
    pthread_t entrance_threads[ENTRANCES];
    entrance_t *entrance;
    for (size_t i = 0; i < ENTRANCES; i++) {
        get_entrance(&shm, i, &entrance);
        boom_gate_t *boom_gate = malloc(sizeof(boom_gate_t));
        boom_gate = &entrance->boom_gate;
        printf("Entrance %d Boom Gate %p\n", i, &entrance->boom_gate);
        pthread_create(&entrance_threads[i], NULL, handle_boom_gate, (void *)boom_gate);
    }

    pthread_mutex_lock(&initialisation_mutex);
    while (ready_threads < (ENTRANCES)) {
        pthread_cond_wait(&initialisation_cond, &initialisation_mutex);
    }
    pthread_mutex_unlock(&initialisation_mutex);
    printf("Finish Init Entrance threads.\n");

    printf("Waiting for simulation to ready.\n");
    sem_wait(simulation_ready_sem);
    printf("=================.\n");

    printf("Manager ready for simulation.\n");
    sem_post(manager_ready_sem);
    printf("=================.\n");
    printf("Start Monitor Thread.\n");
    pthread_create(&monitor_sim_thread, NULL, wait_sim_close, (void *)&shm);

    pthread_join(monitor_sim_thread, NULL);
    printf("=================.\n");
    printf("Joined Monitor Thread.\n");

    printf("=================.\n");
    printf("Joined All Thread.\n");

    printf("Manager Exited.\n");

    for (size_t i = 0; i < ENTRANCES; i++) {
        get_entrance(&shm, i, &entrance);
        boom_gate_t *boom_gate = malloc(sizeof(boom_gate_t));
        boom_gate = &entrance->boom_gate;
        printf("Final Boomgate %p status: %c\n", boom_gate, boom_gate->status);
    }
    return 0;
}
