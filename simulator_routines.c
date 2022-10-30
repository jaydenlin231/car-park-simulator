#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>
#include <semaphore.h>

#include "carpark_details.h"
#include "shared_memory.h"
#include "queue.h"
#include "hashtable.h"
#include "billing.h"
#include "utility.h"
#include "simulator_routines.h"

void *handle_entrance_queue(void *data)
{
    entrance_data_t *entrance_data = (entrance_data_t *)data;
    entrance_t *entrance = entrance_data->entrance;
    queue_t *entrance_queue = entrance_data->entrance_queue;
    pthread_mutex_t *queue_mutex = &entrance_data->queue_mutex;
    pthread_cond_t *cond = &entrance_data->cond;
    sem_t *entrance_LPR_free = &entrance_data->entrance_LPR_free;

    LPR_t *entrance_LPR = &entrance->lpr;
    pthread_mutex_t *LPR_mutex = &entrance_LPR->mutex;
    pthread_cond_t *LPR_cond = &entrance_LPR->cond;

    while (true)
    {
        pthread_mutex_lock(&entrance_LPR->mutex);
        while (entrance_LPR->plate[0] != '\0')
        {
            pthread_cond_wait(&entrance_LPR->cond, &entrance_LPR->mutex);
        }
        pthread_mutex_unlock(&entrance_LPR->mutex);

        pthread_mutex_lock(queue_mutex);
        while (is_empty(entrance_queue))
        {
            pthread_cond_wait(cond, queue_mutex);
        }

        char *first_plate_in_queue = dequeue(entrance_queue);
        pthread_mutex_unlock(queue_mutex);

        // printf("Waiting for 2ms before triggering LPR from Queue:%p with Rego:%s\n", entrance_queue, first_plate_in_queue);
        msleep(2 * TIME_MULTIPLIER);

        pthread_mutex_lock(&entrance_LPR->mutex);
        for (int i = 0; i < 6; i++)
        {
            /* code */
            entrance_LPR->plate[i] = first_plate_in_queue[i];
        }
        pthread_cond_broadcast(&entrance_LPR->cond);
        pthread_mutex_unlock(&entrance_LPR->mutex);
    }
}

void *handle_exit_boomgate(void *data)
{
    exit_t *current_exit = (exit_t *)data;
    LPR_t *exit_LPR = &current_exit->lpr;
    boom_gate_t *boom_gate = &current_exit->boom_gate;
    while (true)
    {
        pthread_mutex_lock(&boom_gate->mutex);
        while (!(boom_gate->status == BG_RAISING || boom_gate->status == BG_LOWERING))
        {
            pthread_cond_wait(&boom_gate->cond, &boom_gate->mutex);
        }
        pthread_mutex_unlock(&boom_gate->mutex);
        if (boom_gate->status == BG_RAISING)
        {
            msleep(10 * TIME_MULTIPLIER);
            pthread_mutex_lock(&boom_gate->mutex);
            boom_gate->status = BG_OPENED;
            pthread_cond_signal(&boom_gate->cond);
            pthread_mutex_unlock(&boom_gate->mutex);
        }
        else if (boom_gate->status == BG_LOWERING)
        {
            msleep(10 * TIME_MULTIPLIER);
            pthread_mutex_lock(&boom_gate->mutex);
            boom_gate->status = BG_CLOSED;
            pthread_cond_signal(&boom_gate->cond);
            pthread_mutex_unlock(&boom_gate->mutex);
        }
    }
    return NULL;
}

void *car_logic(void *data)
{
    level_lpr_data_t *level_lpr_data = (level_lpr_data_t *)data;
    car_t *car = level_lpr_data->car;
    LPR_t *level_lpr = level_lpr_data->level_lpr;
    LPR_t *exit_lpr = level_lpr_data->exit_lpr;
    htab_t *hashtable = level_lpr_data->car_table;

    // Takes 10ms to go to its parking spot
    msleep(10 * TIME_MULTIPLIER);

    // Wait for LPR to be cleared, wait for previous car to finish processing
    pthread_mutex_lock(&level_lpr->mutex);
    while (level_lpr->plate[0] != '\0')
    {
        pthread_cond_wait(&level_lpr->cond, &level_lpr->mutex);
    }
    pthread_mutex_unlock(&level_lpr->mutex);

    // Trigger the level LPR for the first time
    pthread_mutex_lock(&level_lpr->mutex);
    for (int i = 0; i < 6; i++)
    {
        level_lpr->plate[i] = car->plate[i];
    }
    pthread_cond_broadcast(&level_lpr->cond);
    pthread_mutex_unlock(&level_lpr->mutex);

    // Wait 100-10000ms before departing the level
    int rand_park_time = (rand() % (10000 - 100 + 1)) + 100;
    msleep(rand_park_time * TIME_MULTIPLIER);

    // Wait for LPR to be cleared, wait for previous car to finish processing
    pthread_mutex_lock(&level_lpr->mutex);
    while (level_lpr->plate[0] != '\0')
    {
        pthread_cond_wait(&level_lpr->cond, &level_lpr->mutex);
    }
    pthread_mutex_unlock(&level_lpr->mutex);

    // Trigger level LPR for the second time
    pthread_mutex_lock(&level_lpr->mutex);
    for (int i = 0; i < 6; i++)
    {
        level_lpr->plate[i] = car->plate[i];
    }
    pthread_cond_broadcast(&level_lpr->cond);
    pthread_mutex_unlock(&level_lpr->mutex);

    // Takes 10ms from second LPR trigger to exit LPR
    msleep(10 * TIME_MULTIPLIER);

    // Trigger exit LPR when exiting
    pthread_mutex_lock(&exit_lpr->mutex);
    for (int i = 0; i < 6; i++)
    {
        exit_lpr->plate[i] = car->plate[i];
    }
    pthread_cond_broadcast(&exit_lpr->cond);
    pthread_mutex_unlock(&exit_lpr->mutex);

    // Delete car details from hastable for re-entry
    pthread_mutex_lock(&hashtable->mutex);
    htab_delete(hashtable, car->plate);
    pthread_mutex_unlock(&hashtable->mutex);
    // Free resources
    free(car);
    // Finish car simulation
    pthread_exit(NULL);
}

void *handle_entrance_boomgate(void *data)
{
    entrance_data_shm_t *entrance_data_shm = (entrance_data_shm_t *)data;
    shared_memory_t *shm = entrance_data_shm->shm;
    entrance_t *entrance = entrance_data_shm->entrance;
    boom_gate_t *boom_gate = &entrance->boom_gate;
    LPR_t *lpr = &entrance->lpr;
    info_sign_t *sign = &entrance->info_sign;
    htab_t *hashtable = entrance_data_shm->car_table;

    while (true)
    {

        pthread_mutex_lock(&boom_gate->mutex);
        while (!(boom_gate->status == BG_RAISING || boom_gate->status == BG_LOWERING))
        {
            pthread_cond_wait(&boom_gate->cond, &boom_gate->mutex);
        }
        pthread_mutex_unlock(&boom_gate->mutex);

        // IF "EVACUATE" is displayed, let fire alarm open all boom gates,
        // ignoring manager instructions during fire
        if (!(sign->display > '0' && sign->display <= ('0' + LEVELS) && sign->display != '\0'))
        {
            pthread_mutex_lock(&boom_gate->mutex);
            boom_gate->status = BG_OPENED;
            pthread_cond_signal(&boom_gate->cond);
            pthread_mutex_unlock(&boom_gate->mutex);
            continue;
        }
        if (boom_gate->status == BG_RAISING)
        {
            // Boom gates take 10ms to fully open
            msleep(10 * TIME_MULTIPLIER);
            pthread_mutex_lock(&boom_gate->mutex);
            boom_gate->status = BG_OPENED;
            pthread_cond_signal(&boom_gate->cond);
            pthread_mutex_unlock(&boom_gate->mutex);

            // Car logic
            pthread_t car;
            level_lpr_data_t *level_lpr_data = malloc(sizeof(level_lpr_data_t));
            level_lpr_data->car_table = hashtable;
            car_t *car_data = malloc(sizeof(car_t));
            char *plate_copy = malloc(sizeof(char) * (strlen(lpr->plate) + 1));
            strcpy(plate_copy, lpr->plate);
            car_data->plate = plate_copy;

            // zero index level
            int level = (sign->display - '0') - 1;
            car_data->directed_lvl = level;

            // 10% car goes to random level
            if (((rand() % 100) + 1) < 10)
            {
                car_data->actual_lvl = rand() % (LEVELS);
            }
            else
            {
                car_data->actual_lvl = car_data->directed_lvl;
            }
            level_lpr_data->car = car_data;

            // Decide random exit
            int random_exit_number = rand() % EXITS;
            exit_t *random_exit;
            LPR_t *directed_level_LPR;
            get_lpr(shm, car_data->actual_lvl, &directed_level_LPR);
            get_exit(shm, random_exit_number, &random_exit);
            level_lpr_data->level_lpr = directed_level_LPR;
            level_lpr_data->exit_lpr = &random_exit->lpr;
            pthread_create(&car, NULL, car_logic, (void *)level_lpr_data);
        }
        else if (boom_gate->status == BG_LOWERING)
        {
            // Boom gates take 10ms to fully close.
            msleep(10 * TIME_MULTIPLIER);
            pthread_mutex_lock(&boom_gate->mutex);
            boom_gate->status = BG_CLOSED;
            pthread_cond_signal(&boom_gate->cond);
            pthread_mutex_unlock(&boom_gate->mutex);
        }
    }
    return NULL;
}

void *generate_cars(void *arg)
{
    entrance_data_t *entrance_datas = (entrance_data_t *)arg;
    htab_t *hashtable = entrance_datas->car_table;

    pthread_mutex_t lock_rand_num = PTHREAD_MUTEX_INITIALIZER;
    int bool_for_checking = 0;
    char file_location[] = "plates.txt";

    FILE *f = fopen(file_location, "r");
    if (!f)
    {
        perror("File not exising");
        return (NULL);
        bool_for_checking = 1;
    }
    int total_plates;

    // create two d array  //int 100 is 100 plates in .txt
    char **plates = calloc(100, sizeof(char *));

    // each elements in 2d array allocate memory into it
    for (size_t i = 0; i < 100; i++)
    {
        char *each_plateSize = calloc(6, sizeof(char));
        plates[i] = each_plateSize;
    }

    int line = 0;
    char buffer[100];

    while ((line < 100) && (fscanf(f, "%s", buffer) != EOF))
    {
        strcpy(plates[line], buffer);
        line++;
    }
    total_plates = line;

    if (bool_for_checking != 0)
    {
        printf("Couldn't import valid plates!\n");
        return (NULL);
    }
    bool_for_checking = 0;

    while (true)
    {
        // Every 1-100ms, a new car will be generated
        pthread_mutex_lock(&lock_rand_num);
        int rand_car_gen_time = (rand() % 100) + 1;
        pthread_mutex_unlock(&lock_rand_num);
        msleep(rand_car_gen_time * TIME_MULTIPLIER);

        // 50% random number plate, 50% valid number plate
        pthread_mutex_lock(&lock_rand_num);
        int halfChance = rand() % 2;
        pthread_mutex_unlock(&lock_rand_num);

        char six_d_plate[7];

        // Valid number plate from list
        if (halfChance == 1)
        {
            pthread_mutex_lock(&lock_rand_num);

            int index;
            index = (rand() % ((total_plates - 1) + 0 - 1)) + 0;
            pthread_mutex_unlock(&lock_rand_num);
            for (size_t i = 0; i < 3; i++)
            {
                six_d_plate[i] = plates[index][i];
            }

            for (size_t i = 3; i < 6; i++)
            {
                six_d_plate[i] = plates[index][i];
            }
            for (size_t i = 6; i < 7; i++)
            {
                six_d_plate[i] = '\0';
            }
        }
        // Random number plate
        else
        {
            pthread_mutex_lock(&lock_rand_num);
            for (int i = 0; i < 3; i++)
            {
                six_d_plate[i] = '0' + rand() % 9;
                six_d_plate[i + 3] = 'A' + rand() % 26;
            }
            for (size_t i = 6; i < 7; i++)
            {
                six_d_plate[i] = '\0';
            }
            pthread_mutex_unlock(&lock_rand_num);
        }

        // Select random entrance
        int rand_entrance = rand() % ENTRANCES;

        char *plate_string = malloc(sizeof(char) * 7);
        strncpy(plate_string, six_d_plate, 6);
        if (htab_find(hashtable, plate_string) == NULL)
        {
            pthread_mutex_lock(&entrance_datas[rand_entrance].queue_mutex);
            enqueue(entrance_datas[rand_entrance].entrance_queue, plate_string);
            pthread_cond_broadcast(&entrance_datas[rand_entrance].cond);
            pthread_mutex_unlock(&entrance_datas[rand_entrance].queue_mutex);

            pthread_mutex_lock(&hashtable->mutex);
            htab_add(hashtable, plate_string);
            pthread_mutex_unlock(&hashtable->mutex);
        }
    }

    return (NULL);
}

void *wait_manager_close(void *data)
{
    sem_t *manager_ended_sem = (sem_t *)data;
    sem_wait(manager_ended_sem);
    return NULL;
}

char *toArray(int number)
{
    int n = log10(number) + 1;
    int i;
    char *numberArray = calloc(n, sizeof(char));
    for (i = n - 1; i >= 0; --i, number /= 10)
    {
        numberArray[i] = (number % 10) + '0';
    }
    return numberArray;
}

void *sim_fire_sensors(void *data)
{
    sensor_data_t *sensor_datas = (sensor_data_t *)data;
    pthread_mutex_t lock_rand_num = PTHREAD_MUTEX_INITIALIZER;
    int normal;
    char *normal_temp;
    int fixed;
    char *fixed_temp;
    int high_temp_count;
    bool temp_high;
    srand(time(NULL));

    while (1)
    {
        for (int i = 0; i < LEVELS; i++)
        {
            // Default temperature
            char *oldtemp = sensor_datas->level->sensor;
            if (atoi(oldtemp) == 0)
            {
                normal = 25;
                char *normal_temp;
                normal_temp = toArray(normal);
                oldtemp = normal_temp;

                for (int i = 0; i < 1; i++)
                {
                    sensor_datas->level->sensor[i] = 2 + '0';
                    sensor_datas->level->sensor[i + 1] = 5 + '0';
                }
            }
            // Random num ranged between 1-10000
            int rand_num = (rand() % 10000) + 1;

            switch (sensor_datas->type)
            {

            // Normal Mode
            case 'N':

                // Default temperature
                sensor_datas->level->sensor[0] = 2 + '0';
                sensor_datas->level->sensor[1] = 3 + '0';

                // Normal temperature
                if (!temp_high)
                {
                    if (rand_num <= 1)
                    // 0.01% chance of fire
                    {
                        temp_high = true;
                        break;
                    }
                    else
                    // 99.99% chance of normal
                    {
                        // 2-3999
                        if (rand_num < 4000)
                        {
                            sensor_datas->level->sensor[0] = 2 + '0';
                            sensor_datas->level->sensor[1] = 5 + '0';
                            break;
                        }
                        // 4000-5999
                        else if (rand_num < 6000)
                        {
                            sensor_datas->level->sensor[0] = 2 + '0';
                            sensor_datas->level->sensor[1] = 2 + '0';
                            break;
                        }
                        // 6000-7999
                        else if (rand_num < 8000)
                        {
                            sensor_datas->level->sensor[0] = 2 + '0';
                            sensor_datas->level->sensor[1] = 4 + '0';
                            break;
                        }
                        // 7999-1000
                        else if (rand_num <= 10000)
                        {
                            sensor_datas->level->sensor[0] = 2 + '0';
                            sensor_datas->level->sensor[1] = 1 + '0';
                            break;
                        }
                    }
                }
                // Fire
                if (temp_high)
                {
                    high_temp_count++;
                    sensor_datas->level->sensor[0] = 6 + '0';
                    sensor_datas->level->sensor[1] = 1 + '0';

                    // Chance of fire recovery
                    if (high_temp_count > 10000)
                    {
                        temp_high = false;
                        break;
                    }
                    // Continue fire
                    else
                    {
                        if (rand_num < 5000)
                        {
                            sensor_datas->level->sensor[0] = 6 + '0';
                            sensor_datas->level->sensor[1] = 2 + '0';
                            break;
                        }
                        else
                        {
                            sensor_datas->level->sensor[0] = 6 + '0';
                            sensor_datas->level->sensor[1] = 0 + '0';
                            break;
                        }
                    }
                }
                break;
            // Raising Mode
            case 'R':
                // 60% chance of raising temperature
                if (rand_num >= 6000)
                {
                    int c = (atoi(oldtemp)) + 4;
                    char *raise_temp;
                    raise_temp = toArray(c);
                    for (int i = 0; i < 1; i++)
                    {
                        sensor_datas->level->sensor[i] = raise_temp[i];
                        sensor_datas->level->sensor[i + 1] = raise_temp[i + 1];
                    }
                }
                break;
            // Fixed Mode
            case 'F':
                // Constant normal temperature
                for (int i = 0; i < 1; i++)
                {
                    sensor_datas->level->sensor[i] = 2 + '0';
                    sensor_datas->level->sensor[i + 1] = 0 + '0';
                }
                break;
            }
        }
        // The fire alarm system will generate temperature readings every 2ms
        msleep(2 * TIME_MULTIPLIER);
    }
    return NULL;
}