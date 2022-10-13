#pragma once
#include <semaphore.h>

// I didn't do the pthread stuff so just placeholder char[] now

typedef struct LPR {
    // A pthread_mutex_t mutex lock (40 bytes, at bytes 0-39)
    char mutexlock[40];
    // A pthread_cond_t condition variable (48 bytes, at bytes 40-87)
    char cond[48];
    char plate[6];
    char padding[2];

} LPR_t;

typedef struct boomGate {
    // A pthread_mutex_t mutex lock (40 bytes, at bytes 0-39)
    char mutexlock[40];
    // A pthread_cond_t condition variable (48 bytes, at bytes 40-87)
    char cond[48];
    char status;
    char padding[7];

} boomGate_t;

typedef struct infoSign {
    // A pthread_mutex_t mutex lock (40 bytes, at bytes 0-39)
    char mutexlock[40];
    // A pthread_cond_t condition variable (48 bytes, at bytes 40-87)
    char cond[48];
    char display;
    char padding[7];

} infoSign_t;

typedef struct entrance {
    LPR_t lpr;
    boomGate_t boomGate;
    infoSign_t infoSign;

} entrance_t;

typedef struct exit {
    LPR_t lpr;
    boomGate_t boomGate;

} exit_t;

typedef struct level {
    LPR_t lpr;
    // tempSensor_t *tempSensor;
    char sensor[2];
    // alarm_t *alarm;
    char alarm;
    char padding[5];

} level_t;

/**
 * Shared data block
 */
typedef struct data {
    // Entrances
    entrance_t entrance1;
    entrance_t entrance2;
    entrance_t entrance3;
    entrance_t entrance4;
    entrance_t entrance5;

    // Exits
    exit_t exit1;
    exit_t exit2;
    exit_t exit3;
    exit_t exit4;
    exit_t exit5;

    // Levels
    level_t level1;
    level_t level2;
    level_t level3;
    level_t level4;
    level_t level5;

} data_t;

/**
 * A shared memory control structure.
 */
typedef struct shared_memory {
    /// The name of the shared memory object.
    const char *name;

    /// The file descriptor used to manage the shared memory object.
    int fd;

    // List of objects in this shared memory:
    // Car
    // etc
    data_t *data;

} shared_memory_t;