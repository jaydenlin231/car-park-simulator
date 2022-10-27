#pragma once
#include <pthread.h>

#define LEVELS 5
#define ENTRANCES 5
#define EXITS 5
#define NUM_SPOTS_LVL 100

#define BG_RAISING ('R')
#define BG_OPENED ('O')
#define BG_LOWERING ('L')
#define BG_CLOSED ('C')
#define BG_QUIT ('Q')

typedef struct LPR
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    char plate[6];
    int test;

} LPR_t;

typedef struct boom_gate
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    char status;

} boom_gate_t;

typedef struct info_sign
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    char display;

} info_sign_t;

typedef struct entrance
{
    LPR_t lpr;
    boom_gate_t boom_gate;
    info_sign_t info_sign;

} entrance_t;

typedef struct exit
{
    LPR_t lpr;
    boom_gate_t boom_gate;

} exit_t;

typedef struct level
{
    LPR_t lpr;
    // tempSensor_t *tempSensor;
    char sensor[2];
    // alarm_t *alarm;
    char alarm;

} level_t;