#pragma once

#include <semaphore.h>
#include "carpark_details.h"
#include "capacity.h"
#include "billing.h"

typedef struct monitor_entrance
{
    entrance_t *entrance;
    htab_t *hashtable;
    capacity_t *capacity;

} entrance_data_t;

typedef struct sim_man_sem_t
{
    sem_t *simulation_ended_sem;
    sem_t *manager_ended_sem;
} sim_man_sem_t;

typedef struct level_lpr_data
{
    LPR_t *lpr;
    htab_t *hashtable;
    capacity_t *capacity;
    char *temp;
    char *alarm;
    int level;

} level_lpr_data_t;

typedef struct monitor_exit
{
    exit_t *exit;
    htab_t *hashtable;
    int exit_number;
    double *revenue;
    level_t *level;
    pthread_mutex_t revenue_mutex;

} monitor_exit_t;

void *control_boom_gate(boom_gate_t *boom_gate, char update_status);

void *monitor_entrance(void *data);
void *monitor_lpr(void *data);
void *monitor_exit(void *data);
void *wait_sim_close(void *data);
