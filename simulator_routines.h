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
    htab_t *car_table;

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
typedef struct exit_data
{
    exit_t *exit;
    queue_t *exit_queue;
    pthread_mutex_t queue_mutex;
    pthread_cond_t cond;
    sem_t exit_LPR_free;
    htab_t *car_table;

} exit_data_t;

typedef struct entrance_data_shm
{
    entrance_t *entrance;
    shared_memory_t *shm;
    htab_t *car_table;
} entrance_data_shm_t;

typedef struct car
{
    char *plate;
    int directed_lvl;
    int actual_lvl;

} car_t;

typedef struct level_lpr_data
{
    car_t *car;
    LPR_t *level_lpr;
    LPR_t *exit_lpr;
    htab_t *car_table;

} level_lpr_data_t;

void *handle_entrance_boomgate(void *data);
void *handle_exit_boomgate(void *data);
void *handle_entrance_queue(void *data);
void *generate_cars(void *arg);
void *wait_manager_close(void *data);
char * toArray(int number);
void *sim_fire_sensors(void *data);