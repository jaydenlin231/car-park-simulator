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

// Get a car park number from lowest numbered level with a free capacity
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
    // Signal Carpark Full
    capacity->full = true;
    return 0;
}
// Increase car park capacity for a level
void set_capacity(capacity_t *capacity, int level)
{
    if (!capacity->full)
    {
        capacity->curr_capacity[level - 1] += 1;
        if (!get_empty_spot(capacity))
        {
            capacity->full = true;
        }
    }
}

// Free car park capacity from a level
void free_carpark_space(capacity_t *capacity, int level)
{
    int cap = capacity->curr_capacity[level - 1] - 1;
    if (cap >= 0)
    {
        capacity->curr_capacity[level - 1] = cap;
        capacity->full = false;
    }
    else
    {
        capacity->curr_capacity[level - 1] = 0;
        capacity->full = false;
    }
}

// Print the current capacity of all levels
void print_capacity(capacity_t *capacity)
{
    printf("Capacity: [ ");
    for (int i = 0; i < LEVELS; i++)
    {
        printf("%d ", capacity->curr_capacity[i]);
    }
    printf("]\t");
}