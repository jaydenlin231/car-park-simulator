#include <stdio.h>

#include "carpark_details.h"
#include "capacity.h"

void init_capacity(capacity_t *capacity)
{
    pthread_mutex_init(&capacity->mutex, NULL);
    capacity->curr_capacity[LEVELS] = 0;
    capacity->full = false;
    capacity->max_cap_per_lvl = NUM_SPOTS_LVL;
}

int get_empty_spot(capacity_t *capacity)
{
    if (!capacity->full)
    {
        for (int i = 0; i < LEVELS; i++)
        {
            if (capacity->curr_capacity[i] < capacity->max_cap_per_lvl)
            {
                return i + 1;
            }
        }
    }
    // Carpark Full
    capacity->full = true;
    return NULL;
}

void set_capacity(capacity_t *capacity, int level)
{
    if (!capacity->full)
    {
        capacity->curr_capacity[level - 1] += 1;
    }
}

void free_carpark_space(capacity_t *capacity, int level)
{
    capacity->curr_capacity[level - 1] -= 1;
    capacity->full = false;
}

void print_capacity(capacity_t *capacity)
{
    printf("Capacity: [ ");
    for (int i = 0; i < LEVELS; i++)
    {
        printf("%d ", capacity->curr_capacity[i]);
    }
    printf("]\n");
}