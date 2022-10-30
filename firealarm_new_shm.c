#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <semaphore.h>

#include "carpark_details.h"
#include "shared_memory.h"
#include "utility.h"
#define ALARM_READY_SEM_NAME "/ALARM_READY"

// #define SEG_NAME "/PARKING"
// #define SEG_SIZE 2920
// #define QUEUE_SIZE 10000

// #define TIME_SCALE 1
// Structre of carpark
// #define ENTRANCES 5
// #define EXITS 5
// #define LEVELS 5
// #define VEHICLES_PER_LVL 20

// Firealarm defines
#define MEDIAN_WINDOW 5
#define TEMPCHANGE_WINDOW 30
int lv_id[LEVELS];

#define FILE_LOC "plates.txt"

int16_t shm_fd;
// void *shm;
static shared_memory_t shm;

int alarm_active = 0;
pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t alarm_condvar = PTHREAD_COND_INITIALIZER;

// struct boomgate
// {
//  pthread_mutex_t m;
//  pthread_cond_t c;
//  char s;
// };
// struct parkingsign
// {
//  pthread_mutex_t m;
//  pthread_cond_t c;
//  char display;
// };

struct tempnode
{
    int temperature;
    struct tempnode *next;
};

struct tempnode *deletenodes(struct tempnode *templist, int after)
{
    if (templist->next)
    {
        templist->next = deletenodes(templist->next, after - 1);
    }
    if (after <= 0)
    {
        free(templist);
        return NULL;
    }
    return templist;
}

int compare(const void *first, const void *second)
{
    return *((const int *)first) - *((const int *)second);
}

void *tempmonitor(void *levelArg)
{
    int level = (*(int *)levelArg);
    level_t *current_level;
    get_level(&shm, level, &current_level);
    char *sensor = current_level->sensor;

    struct tempnode *templist = calloc(1, sizeof(struct tempnode));
    struct tempnode *newtemp;
    struct tempnode *medianlist = calloc(1, sizeof(struct tempnode));
    struct tempnode *oldesttemp;
    int count;
    // int addr;
    int temp;
    int mediantemp;
    int hightemps;
    char *sign;

    // intptr_t level = (intptr_t) levelArg;
    while (alarm_active == 0)
    { // !!NASA Power of 10: #2 (loops have fixed bounds)!
        // Calculate address of temperature sensor
        // addr = 104 * level + 2496;
        char firstChar = sensor[0] - '0';
        char secondChar = sensor[1] - '0';
        temp = firstChar * 10 + secondChar;
        current_level->alarm = '0';

        // Add temperature to beginning of linked lis
        // newtemp = malloc(sizeof(struct tempnode));
        newtemp = calloc(1, sizeof(struct tempnode));
        newtemp->temperature = temp;
        newtemp->next = templist;
        templist = newtemp;

        // Delete nodes after 5th
        deletenodes(templist, MEDIAN_WINDOW);

        // Count nodes
        count = 0;
        for (struct tempnode *t = templist; t != NULL; t = t->next)
        {
            count++;
        }

        // Delete nodes after 5th
        // if(count > 5){
        //     deletenodes(templist, MEDIAN_WINDOW);
        // }

        if (count == MEDIAN_WINDOW)
        { // Temperatures are only counted once we have 5 samples
            // int *sorttemp = malloc(sizeof(int) * MEDIAN_WINDOW);
            int *sorttemp = calloc(MEDIAN_WINDOW, sizeof(int));
            count = 0;
            for (struct tempnode *t = templist; t != NULL; t = t->next)
            {
                // count++;
                sorttemp[count++] = t->temperature;
                // printf("count is %d\n", count);
            }
            qsort(sorttemp, MEDIAN_WINDOW, sizeof(int), compare);
            mediantemp = sorttemp[(MEDIAN_WINDOW - 1) / 2];
            free(sorttemp);

            // Add median temp to linked list
            // newtemp = malloc(sizeof(struct tempnode));
            newtemp = calloc(1, sizeof(struct tempnode));
            newtemp->temperature = mediantemp;
            // printf("Median Temp:%d\n", mediantemp);
            newtemp->next = medianlist;
            medianlist = newtemp;

            // Delete nodes after 30th
            deletenodes(medianlist, TEMPCHANGE_WINDOW);

            // Count nodes
            count = 0;
            hightemps = 0;

            for (struct tempnode *t = medianlist; t != NULL; t = t->next)
            {
                // Temperatures of 58 degrees and higher are a concern
                if (t->temperature >= 58)
                    hightemps++;
                // Store the oldest temperature for rate-of-rise detection
                oldesttemp = t;
                count++;
                if (count > 30)
                {
                    break;
                }
            }

            if (count == TEMPCHANGE_WINDOW)
            {
                // If 90% of the last 30 temperatures are >= 58 degrees,
                // this is considered a high temperature. Raise the alarm
                if (hightemps >= TEMPCHANGE_WINDOW * 0.9)
                {
                    printf("Option 1: High Temperature Detected.\n");
                    alarm_active = 1;
                    // sign = shm + addr + 2;
                    // *sign = 1;
                    current_level->alarm = 1;
                }

                // If the newest temp is >= 8 degrees higher than the oldest
                // temp (out of the last 30), this is a high rate-of-rise.
                // Raise the alarm
                if (templist->temperature - oldesttemp->temperature >= 8 && oldesttemp->temperature != 0)
                {

                    printf("Option 2: High Rate-of-Rise Detected.\n");
                    alarm_active = 1;
                    // sign = shm + addr + 2;
                    // *sign = 1;
                    current_level->alarm = 1;
                }
            }
        }

        usleep(2000);
    }
    return NULL;
}

void *open_en_boomgate(void *arg)
{
    // struct boomgate *bg = arg;
    int i = *((int *)arg);
    entrance_t *entrance;
    get_entrance(&shm, i, &entrance);
    boom_gate_t *bg = &entrance->boom_gate;
    // struct boomgate *bg = shm + 288 * i + 96;

    do
    {
        pthread_mutex_lock(&bg->mutex);
        if (bg->status == 'C')
        {
            bg->status = 'R';
            pthread_mutex_unlock(&bg->mutex);
            pthread_cond_broadcast(&bg->cond);
        }
        else if (bg->status == 'O')
        {
            pthread_cond_wait(&bg->cond, &bg->mutex);
        }
        else
        {
            pthread_mutex_unlock(&bg->mutex);
        }
    } while (bg->status != 'O');
    return NULL;
}

void *open_ex_boomgate(void *arg)
{
    // struct boomgate *bg = arg;
    int i = *((int *)arg);
    // struct boomgate *bg = shm + 192 * i + 1536;
    exit_t *exit;
    get_exit(&shm, i, &exit);
    boom_gate_t *bg = &exit->boom_gate;

    do
    {
        pthread_mutex_lock(&bg->mutex);
        if (bg->status != 'O')
        {
            bg->status = 'R';
            pthread_mutex_unlock(&bg->mutex);
            pthread_cond_broadcast(&bg->cond);
        }
        else if (bg->status == 'O')
        {
            pthread_cond_wait(&bg->cond, &bg->mutex);
        }
        else
        {
            pthread_mutex_unlock(&bg->mutex);
        }
    } while (bg->status != 'O');
    return NULL;
}

int main()
{
    // sem_t *alarm_ready_sem = sem_open(ALARM_READY_SEM_NAME, 0);
    printf("Fire Alarm Started\n");
    int en_id[ENTRANCES];
    int ex_id[EXITS];
    // shm_fd = shm_open(SEG_NAME, O_RDWR, 0666);
    // ftruncate(shm_fd, SEG_SIZE);
    // shm = (void *)mmap(0, SEG_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    if (get_shared_object(&shm))
    {
        // sem_post(shm_established_sem);
        printf("Fire Alarm connected to shm.\n");
    }
    else
    {
        perror("Shared memory connection failed.");
        return -1;
    }

    // pthread_t *threads = malloc(sizeof(pthread_t) * LEVELS);
    pthread_t *threads[LEVELS];
    for (intptr_t i = 0; i < LEVELS; i++)
    {
        lv_id[i] = i;

        pthread_create(&threads[i], NULL, tempmonitor, (void *)&lv_id[i]);
    }
    while (!alarm_active)
    {
        usleep(1000);
    }
    fprintf(stderr, "*** ALARM ACTIVE ***\n");
    if (alarm_active)
    {
        // Handle the alarm system and open boom gates
        // Activate alarms on all levels
        level_t *level;
        for (int i = 0; i < LEVELS; i++)
        {
            //  int addr = 0150 * i + 2498; // no octat
            //  char *alarm_trigger = (char *)shm + addr;
            // *alarm_trigger = '1';
            get_level(&shm, i, &level);
            level->alarm = '1';
        }
    }
    // Open up all boom gates
    // pthread_t *boomgatethreads = malloc(sizeof(pthread_t) * (ENTRANCES + EXITS));
    pthread_t *entrance_threads[ENTRANCES];
    for (int i = 0; i < ENTRANCES; i++)
    {
        en_id[i] = i;

        // int addr = 288 * i + 96;
        // volatile struct boomgate *bg = shm + addr;
        // pthread_cond_broadcast((void *)&bg->cond);
        // pthread_create(boomgatethreads + i, NULL, openboomgate, (void*)bg);
        // pthread_create(boomgatethreads + i, NULL, open_en_boomgate, (void *)&en_id[i]);
        pthread_create(&entrance_threads[i], NULL, open_en_boomgate, (void *)&en_id[i]);
    }
    pthread_t *exit_threads[EXITS];
    for (int i = 0; i < EXITS; i++)
    {
        ex_id[i] = i;

        // int addr = 192 * i + 1536;
        // volatile struct boomgate *bg = shm + addr;
        // pthread_cond_broadcast((void *)&bg->cond);
        // pthread_create(boomgatethreads + ENTRANCES + i, NULL, openboomgate, (void*)bg);
        pthread_create(&exit_threads[i], NULL, open_ex_boomgate, (void *)&ex_id[i]);
    }

    // Show evacuation message on an endless loop
    while (alarm_active == 1)
    {
        // if ((*(char *)(shm + 2919)) == 1)
        // {
        //  break;
        // }
        char *evacmessage = "EVACUATE ";
        for (char *p = evacmessage; *p != '\0'; p++)
        {
            entrance_t *entrance;
            for (int i = 0; i < ENTRANCES; i++)
            {
                // int addr = 288 * i + 192;
                get_entrance(&shm, i, &entrance);
                info_sign_t *sign = &entrance->info_sign;

                // struct parkingsign *sign = shm + addr;
                pthread_mutex_lock((void *)&sign->mutex);
                sign->display = *p;
                pthread_cond_broadcast((void *)&sign->cond);
                pthread_mutex_unlock((void *)&sign->mutex);
            }
            usleep(20000 * TIME_MULTIPLIER);
        }
    }

    for (int i = 0; i < LEVELS; i++)
    {
        pthread_join(threads[i], NULL);
    }

    // munmap((void *)shm, 2920);
    // close(shm_fd);
    return 0;
}