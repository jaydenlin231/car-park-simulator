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


// Firealarm defines
#define MEDIAN_WINDOW 5
#define TEMPCHANGE_WINDOW 30
int lv_id[LEVELS];

#define FILE_LOC "plates.txt"

int16_t shm_fd;
static shared_memory_t shm;

int alarm_active = 0;
pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t alarm_condvar = PTHREAD_COND_INITIALIZER;

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
    int temp;
    int mediantemp;
    int hightemps;
    char *sign;

    // intptr_t level = (intptr_t) levelArg;
    while (alarm_active == 0)
    { // !!NASA Power of 10: #2 (loops have fixed bounds)!
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

        if (count == MEDIAN_WINDOW)
        { // Temperatures are only counted once we have 5 samples
            int *sorttemp = calloc(MEDIAN_WINDOW, sizeof(int));
            count = 0;
            for (struct tempnode *t = templist; t != NULL; t = t->next)
            {
                sorttemp[count++] = t->temperature;
            }
            qsort(sorttemp, MEDIAN_WINDOW, sizeof(int), compare);
            mediantemp = sorttemp[(MEDIAN_WINDOW - 1) / 2];
            free(sorttemp);

            // Add median temp to linked list
            newtemp = calloc(1, sizeof(struct tempnode));
            newtemp->temperature = mediantemp;
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
                    current_level->alarm = 1;
                }

                // If the newest temp is >= 8 degrees higher than the oldest
                // temp (out of the last 30), this is a high rate-of-rise.
                // Raise the alarm
                if (templist->temperature - oldesttemp->temperature >= 8 && oldesttemp->temperature != 0)
                {

                    printf("Option 2: High Rate-of-Rise Detected.\n");
                    alarm_active = 1;
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
    int i = *((int *)arg);
    entrance_t *entrance;
    get_entrance(&shm, i, &entrance);
    boom_gate_t *bg = &entrance->boom_gate;

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
    int i = *((int *)arg);
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
    printf("Fire Alarm Started\n");
    int en_id[ENTRANCES];
    int ex_id[EXITS];

    if (get_shared_object(&shm))
    {
        printf("Fire Alarm connected to shm.\n");
    }
    else
    {
        perror("Shared memory connection failed.");
        return -1;
    }

    pthread_t threads[LEVELS];
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
            get_level(&shm, i, &level);
            level->alarm = '1';
        }
    }
    // Open up all boom gates
    pthread_t entrance_threads[ENTRANCES];
    for (int i = 0; i < ENTRANCES; i++)
    {
        en_id[i] = i;
        pthread_create(&entrance_threads[i], NULL, open_en_boomgate, (void *)&en_id[i]);
    }
    pthread_t exit_threads[EXITS];
    for (int i = 0; i < EXITS; i++)
    {
        ex_id[i] = i;
        pthread_create(&exit_threads[i], NULL, open_ex_boomgate, (void *)&ex_id[i]);
    }

    // Show evacuation message on an endless loop
    while (alarm_active == 1)
    {
        char *evacmessage = "EVACUATE ";
        for (char *p = evacmessage; *p != '\0'; p++)
        {
            entrance_t *entrance;
            for (int i = 0; i < ENTRANCES; i++)
            {
                get_entrance(&shm, i, &entrance);
                info_sign_t *sign = &entrance->info_sign;

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

    return 0;
}