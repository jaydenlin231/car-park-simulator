#pragma once

#include <semaphore.h>
#include "carpark_details.h"
#include "capacity.h"

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

void *control_boom_gate(boom_gate_t *boom_gate, char update_status);

void *monitor_entrance(void *data);

// void *handle_boom_gate(void *data);

void *wait_sim_close(void *data);
