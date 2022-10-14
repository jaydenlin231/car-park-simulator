#pragma once
#include "carpark_types.h"

/**
 * Shared data block
 */
typedef struct data {
    // Entrances
    entrance_t entrances[ENTRANCES];

    // Exits
    exit_t exits[EXITS];

    // Levels
    level_t levels[LEVELS];

    int test;

} data_t;

/**
 * A shared memory control structure.
 */
typedef struct shared_memory {
    /// The name of the shared memory object.
    const char *name;

    /// The file descriptor used to manage the shared memory object.
    int fd;

    data_t *data;

} shared_memory_t;

bool create_shared_object(shared_memory_t *shm);
void destroy_shared_object(shared_memory_t *shm);
bool get_shared_object(shared_memory_t *shm);