#pragma once
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct capacity
{
    pthread_mutex_t mutex;
    int curr_capacity[LEVELS];
    int max_cap_per_lvl;
    bool full;

} capacity_t;

void init_capacity(capacity_t *capacity);
void free_carpark_space(capacity_t *capacity, int level);
int get_set_empty_spot(capacity_t *capacity);
void print_capacity(capacity_t *capacity);

