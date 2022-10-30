#pragma once
#include "carpark_details.h"

/**
 * Shared data block
 */
typedef struct data
{
    // Entrances
    entrance_t entrances[ENTRANCES];

    // Exits
    exit_t exits[EXITS];

    // Levels
    level_t levels[LEVELS];

} data_t;

/**
 * A shared memory control structure.
 */
typedef struct shared_memory
{
    /// The name of the shared memory object.
    const char *name;

    /// The file descriptor used to manage the shared memory object.
    int fd;

    data_t *data;

} shared_memory_t;

bool create_shared_object(shared_memory_t *shm);
void destroy_shared_object(shared_memory_t *shm);
bool get_shared_object(shared_memory_t *shm);
void init_shared_memory_data(shared_memory_t *shm);
void clean_shared_memory_data(shared_memory_t *shm);

void get_entrance(shared_memory_t *shm, int i, entrance_t **entrance);
void get_exit(shared_memory_t *shm, int i, exit_t **exit);
void get_level(shared_memory_t *shm, int i, level_t **level);
void get_lpr(shared_memory_t *shm, int i, LPR_t **lpr);

void init_entrance_data(shared_memory_t *shm);

