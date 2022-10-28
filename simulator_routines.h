#pragma once

#include "carpark_details.h"
#include <semaphore.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>


typedef struct entrance_data
{
    entrance_t *entrance;
    queue_t *entrance_queue;
    pthread_mutex_t queue_mutex;
    pthread_cond_t cond;
    sem_t entrance_LPR_free;

} entrance_data_t;

typedef struct sensor_data
{
    entrance_t * entrance;
    level_t * level;
    exit_t * exit;
    pthread_mutex_t m;
    pthread_cond_t c;
    sem_t sim_to_man;
    char type;
    
}sensor_data_t; 

void *handle_boom_gate(void *data);
void *handle_entrance_queue(void *data);
void *generate_cars(void *arg);
void *wait_manager_close(void *data);
char * toArray(int number);
void *sim_fire_sensors(void *data);