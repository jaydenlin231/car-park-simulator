#pragma once

#include "carpark_details.h"
#include <semaphore.h>

typedef struct entrance_data
{
    entrance_t *entrance;
    queue_t *entrance_queue;
    pthread_mutex_t queue_mutex;
    pthread_cond_t cond;
    sem_t entrance_LPR_free;

} entrance_data_t;

typedef struct car
{
    char *plate;
    int directed_lvl;
    int actual_lvl;

} car_t;

typedef struct level_lpr_data
{
    car_t *car;
    LPR_t *lpr;

} level_lpr_data_t;

void *handle_entrance_boomgate(void *data);
void *handle_entrance_queue(void *data);
void *generate_cars(void *arg);
void *wait_manager_close(void *data);