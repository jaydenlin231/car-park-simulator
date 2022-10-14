#pragma once
#include <pthread.h>
// #include <semaphore.h>

#define LEVELS 5
#define ENTRANCES 5
#define EXITS 5

// pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
// pthread_cond_t alarm_condvar = PTHREAD_COND_INITIALIZER;

// I didn't do the pthread stuff so just placeholder char[] now

typedef struct LPR {
    pthread_mutex_t m;
    pthread_cond_t c;
    char plate[6];
    char padding[2];

} LPR_t;

typedef struct boom_gate {
    pthread_mutex_t m;
    pthread_cond_t c;
    char status;
    char padding[7];

} boomGate_t;

typedef struct info_sign {
    pthread_mutex_t m;
    pthread_cond_t c;
    char display;
    char padding[7];

} infoSign_t;

typedef struct entrance {
    LPR_t lpr;
    boomGate_t boom_gate;
    infoSign_t info_sign;

} entrance_t;

typedef struct exit {
    LPR_t lpr;
    boomGate_t boom_gate;

} exit_t;

typedef struct level {
    LPR_t lpr;
    // tempSensor_t *tempSensor;
    char sensor[2];
    // alarm_t *alarm;
    char alarm;
    char padding[5];

} level_t;