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
        // printf("\n\tManage entrance loop ran\n");

        if (pthread_mutex_lock(&entrance_LPR->mutex) != 0)
        {
            perror("pthread_mutex_lock(&LPR->mutex)");
            exit(1);
        };
        while (entrance_LPR->plate[0] != '\0')
        {
            // printf("\t\tCond Wait LPR NULL, currently: %s\n", entrance_LPR->plate);
            pthread_cond_wait(&entrance_LPR->cond, &entrance_LPR->mutex);
        }
        if (pthread_mutex_unlock(&entrance_LPR->mutex) != 0)
        {
            perror("pthread_mutex_lock(&LPR->mutex)");
            exit(1);
        };

        // printf("\t\tQueue:%p entrance_LPR_free\n", entrance_queue);

        if (pthread_mutex_lock(queue_mutex) != 0)
        {
            exit(1);
        };
        // printf("Is Empty: %d\n", entrance_data->queue_is_empty);
        while (is_empty(entrance_queue))
        {
            // printf("\t\tQueue:%p COND WAIT queue not empty, current is_empty status:%d\n", entrance_queue, is_empty(entrance_queue));
            pthread_cond_wait(cond, queue_mutex);
        }
        // pthread_mutex_unlock(queue_mutex);

        // pthread_mutex_lock(queue_mutex);
        char *first_plate_in_queue = dequeue(entrance_queue);
        // printf("Dequeue from Queue:%p - %s\n", entrance_queue, first_plate_in_queue);
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
        // printf("\t\tEntrance LPR set as: %s\n", entrance_LPR->plate);
    }
}

void *handle_exit_boomgate(void *data)
{
    exit_t *current_exit = (exit_t *)data;
    // exit_t *current_exit = exit_data->exit;
    LPR_t *exit_LPR = &current_exit->lpr;
    boom_gate_t *boom_gate = &current_exit->boom_gate;
    // pthread_mutex_t *LPR_mutex = &exit_LPR->mutex;
    // pthread_cond_t *LPR_cond = &exit_LPR->cond;
    while (true)
    {
        // printf("\tBoom Gate %p Monitor Loop ran %d times.\n", boom_gate, ++test);

        if (pthread_mutex_lock(&boom_gate->mutex) != 0)
        {
            exit(1);
        };
        while (!(boom_gate->status == BG_RAISING || boom_gate->status == BG_LOWERING))
        {
            // printf("\tBoom Gate %p Cond Wait BG_RAISING or BG_LOWERING. Current Status: %c.\n", boom_gate, boom_gate->status);
            if (pthread_cond_wait(&boom_gate->cond, &boom_gate->mutex) != 0)
            {
                exit(1);
            };
        }
        if (pthread_mutex_unlock(&boom_gate->mutex) != 0)
        {
            exit(1);
        };
        // printf("Boom Gate %p Received Instruction Status: %c.\n", boom_gate, boom_gate->status);
        if (boom_gate->status == BG_RAISING)
        {
            // printf("Raising Boom Gate %p...\n", boom_gate);
            // printf("Opening: 10 ms\n");
            msleep(10 * TIME_MULTIPLIER);
            pthread_mutex_lock(&boom_gate->mutex);
            boom_gate->status = BG_OPENED;
            // printf("Boom Gate %p Opened\n", boom_gate);
            pthread_cond_signal(&boom_gate->cond);
            pthread_mutex_unlock(&boom_gate->mutex);
        }
        else if (boom_gate->status == BG_LOWERING)
        {
            // printf("Lowering Boom Gate %p...\n", boom_gate);
            // printf("Lowering: 10 ms\n");
            msleep(10 * TIME_MULTIPLIER);
            pthread_mutex_lock(&boom_gate->mutex);
            boom_gate->status = BG_CLOSED;
            // printf("Boom Gate %p Closed\n", boom_gate);
            pthread_cond_signal(&boom_gate->cond);
            pthread_mutex_unlock(&boom_gate->mutex);
        }
    }
    printf("Boom Gate Quit before broadcast %p...\n", boom_gate);
    return NULL;
}

void *car_logic(void *data)
{

    level_lpr_data_t *level_lpr_data = (level_lpr_data_t *)data;
    car_t *car = level_lpr_data->car;
    LPR_t *level_lpr = level_lpr_data->level_lpr;
    LPR_t *exit_lpr = level_lpr_data->exit_lpr;
    htab_t *hashtable = level_lpr_data->car_table;
    msleep(10 * TIME_MULTIPLIER); // Takes 10ms to go to its parking spot

    // printf("Car logic started\n");

    // printf("Attemp to change level LPR\n");
    pthread_mutex_lock(&level_lpr->mutex);
    while (level_lpr->plate[0] != NULL)
    {
        // printf("\t\tCond Wait LPR not NULL, currently: %s\n", level_lpr->plate);
        pthread_cond_wait(&level_lpr->cond, &level_lpr->mutex);
    }
    pthread_mutex_unlock(&level_lpr->mutex);

    pthread_mutex_lock(&level_lpr->mutex);
    for (int i = 0; i < 6; i++)
    {
        level_lpr->plate[i] = car->plate[i];
    }
    pthread_cond_broadcast(&level_lpr->cond);
    pthread_mutex_unlock(&level_lpr->mutex);
    // Trigger level LPR here prob with mutex locks and conds
    // pthread_mutex_lock
    // Then the manager sees the level LPR has been triggerd

    // Then increases capacity by 1 at that level and start billing

    // printf("***********************************\n");
    // printf("level lpr reads: %s\n", lpr->plate);
    // printf("The car: %s is parked at: %d\n", car->plate, car->directed_lvl);
    // printf("***********************************\n");

    // printf("The car: %s triggered level: %d LPR\n", car->plate, car->directed_lvl);

    int rand_park_time = (rand() % (10000 - 10 + 1)) + 10;
    // msleep(rand_park_time * TIME_MULTIPLIER); // Park for 10-10000ms
    msleep(100); // Park for 10-10000ms
    // printf("Parking for %d ms\n", rand_park_time);
    pthread_mutex_lock(&level_lpr->mutex);
    while (level_lpr->plate[0] != NULL)
    {
        // printf("\t\tCond Wait LPR not NULL, currently: %s\n", level_lpr->plate);
        pthread_cond_wait(&level_lpr->cond, &level_lpr->mutex);
    }
    pthread_mutex_unlock(&level_lpr->mutex);

    // // Trigger level LPR again when exiting
    pthread_mutex_lock(&level_lpr->mutex);
    for (int i = 0; i < 6; i++)
    {
        level_lpr->plate[i] = car->plate[i];
    }
    pthread_cond_broadcast(&level_lpr->cond);
    pthread_mutex_unlock(&level_lpr->mutex);

    // Takes 10ms from second LPR trigger to Exit LPR
    msleep(10 * TIME_MULTIPLIER);

    pthread_mutex_lock(&exit_lpr->mutex);
    for (int i = 0; i < 6; i++)
    {
        exit_lpr->plate[i] = car->plate[i];
    }
    pthread_cond_broadcast(&exit_lpr->cond);
    pthread_mutex_unlock(&exit_lpr->mutex);

    pthread_mutex_lock(&hashtable->mutex);
    htab_delete(hashtable, car->plate);
    pthread_mutex_unlock(&hashtable->mutex);
    free(car);
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

    printf("Created Boom Gate %p Thread\n", boom_gate);

    // pthread_mutex_lock(&initialisation_mutex);
    // ready_threads++;
    // pthread_cond_broadcast(&initialisation_cond);
    // pthread_mutex_unlock(&initialisation_mutex);
    // int num = 0;
    int test = 0;
    while (true)
    {
        // printf("\tBoom Gate %p Monitor Loop ran %d times.\n", boom_gate, ++test);

        if (pthread_mutex_lock(&boom_gate->mutex) != 0)
        {
            exit(1);
        };
        while (!(boom_gate->status == BG_RAISING || boom_gate->status == BG_LOWERING))
        {
            // printf("\tBoom Gate %p Cond Wait BG_RAISING or BG_LOWERING. Current Status: %c.\n", boom_gate, boom_gate->status);
            if (pthread_cond_wait(&boom_gate->cond, &boom_gate->mutex) != 0)
            {
                exit(1);
            };
        }
        if (pthread_mutex_unlock(&boom_gate->mutex) != 0)
        {
            exit(1);
        };
        // printf("Boom Gate %p Received Instruction Status: %c.\n", boom_gate, boom_gate->status);
        if (boom_gate->status == BG_RAISING)
        {
            // printf("Raising Boom Gate %p...\n", boom_gate);
            // printf("Opening: 10 ms\n");
            msleep(10 * TIME_MULTIPLIER);
            pthread_mutex_lock(&boom_gate->mutex);
            boom_gate->status = BG_OPENED;
            // printf("Boom Gate %p Opened\n", boom_gate);
            pthread_cond_signal(&boom_gate->cond);
            pthread_mutex_unlock(&boom_gate->mutex);

            // Car logic stuff
            pthread_t car;
            level_lpr_data_t *level_lpr_data = malloc(sizeof(level_lpr_data_t));
            level_lpr_data->car_table = hashtable;
            car_t *car_data = malloc(sizeof(car_t));
            char *plate_copy = malloc(sizeof(char) * (strlen(lpr->plate) + 1));
            strcpy(plate_copy, lpr->plate);
            // htab_add(hashtable, plate_copy);
            car_data->plate = plate_copy;
            // zero index level
            int level = (sign->display - '0') - 1;
            car_data->directed_lvl = level;
            if (((rand() % 100) + 1) < 10) // 10% car goes to random level
            {
                car_data->actual_lvl = rand() % (LEVELS);
            }
            else
            {
                car_data->actual_lvl = car_data->directed_lvl;
            }
            // car_data->actual_lvl = car_data->directed_lvl;
            level_lpr_data->car = car_data;
            int random_exit_number = rand() % EXITS;
            exit_t *random_exit;
            LPR_t *directed_level_LPR;
            get_lpr(shm, car_data->actual_lvl, &directed_level_LPR);
            get_exit(shm, random_exit_number, &random_exit);
            level_lpr_data->level_lpr = directed_level_LPR;
            level_lpr_data->exit_lpr = &random_exit->lpr;
            // printf("Car wants to go to zero index: %d\n", level);
            pthread_create(&car, NULL, car_logic, (void *)level_lpr_data);
        }
        else if (boom_gate->status == BG_LOWERING)
        {
            // printf("Lowering Boom Gate %p...\n", boom_gate);
            // printf("Lowering: 10 ms\n");
            msleep(10 * TIME_MULTIPLIER);
            pthread_mutex_lock(&boom_gate->mutex);
            boom_gate->status = BG_CLOSED;
            // printf("Boom Gate %p Closed\n", boom_gate);
            pthread_cond_signal(&boom_gate->cond);
            pthread_mutex_unlock(&boom_gate->mutex);
        }
    }
    printf("Boom Gate Quit before broadcast %p...\n", boom_gate);
    return NULL;
}

void *generate_cars(void *arg)
{
    entrance_data_t *entrance_datas = (entrance_data_t *)arg;
    htab_t *hashtable = entrance_datas->car_table;
    // queue_t *entrance_queue = entrance_data[]->entrance_queue;

    pthread_mutex_t lock_rand_num = PTHREAD_MUTEX_INITIALIZER;
    int bool_for_checking = 0;
    char file_location[] = "plates.txt";
    // printf("%s\n", file_location );
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
    // int plates_len;
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
        // printf("Read plate %s\n", buffer);

        strcpy(plates[line], buffer);
        line++;
    }
    total_plates = line;
    // printf("%d\n", total_plates);
    // plates_len = line;
    if (bool_for_checking != 0)
    {
        printf("Couldn't import valid plates!\n");
        return (NULL);
    }
    bool_for_checking = 0;

    while (true)
    {
        pthread_mutex_lock(&lock_rand_num);
        int rand_car_gen_time = (rand() % 100) + 1;
        pthread_mutex_unlock(&lock_rand_num);

        msleep((rand_car_gen_time / 10) * TIME_MULTIPLIER);
        // msleep(100);
        // char result;
        pthread_mutex_lock(&lock_rand_num);
        int halfChance = rand() % 2;
        pthread_mutex_unlock(&lock_rand_num);

        char six_d_plate[7];
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
        int rand_entrance = rand() % ENTRANCES;

        char *plate_string = malloc(sizeof(char) * 7);
        // plate_string[6] = '\0';
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
        else
        {
            // printf("%s has already been generated\n", plate_string);
        }

        // printf("%s queued to queue #%d at %Lf\n", six_d_plate, rand_entrance, get_time());
        // for (int i = 0; i < ENTRANCES; i++)
        // {
        //     printf("Entrace %d: ", i);
        //     print_queue(entrance_datas[i].entrance_queue);
        // }
        // printf("\n");
    }

    return (NULL);
}

void *wait_manager_close(void *data)
{
    sem_t *manager_ended_sem = (sem_t *)data;
    printf("Manager monitor thread waiting.\n");
    sem_wait(manager_ended_sem);
    printf("Manager monitor thread Woke\n");
    return NULL;
}

// char *toArray(int number)
// {
//     int n = log10(number) + 1;
//     int i;
//     char *numberArray = calloc(n, sizeof(char));
//     for (i = n - 1; i >= 0; --i, number /= 10)
//     {
//         numberArray[i] = (number % 10) + '0';
//     }
//     return numberArray;
// }

// void *sim_fire_sensors(void *data)
// {
//     sensor_data_t *sensor_datas = (sensor_data_t *)data;
//     pthread_mutex_t lock_rand_num = PTHREAD_MUTEX_INITIALIZER;
//     int normal;
//     char *normal_temp;
//     // int a ;
//     int fixed;
//     char *fixed_temp;
//     int high_temp_count;
//     bool temp_high;
//     srand(time(NULL));

//     while (1)
//     {
//         for (int i = 0; i < LEVELS; i++)
//         {

//             char *oldtemp = sensor_datas->level->sensor;
//             if (atoi(oldtemp) == 0)
//             {
//                 normal = 25;
//                 char *normal_temp;
//                 normal_temp = toArray(normal);
//                 oldtemp = normal_temp;
//                 // for(int i = 0 ; i < 2; i++)
//                 // {
//                 //     sensor_datas->level->sensor[i] = normal_temp[i];
//                 //     // printf("%s ", sensor_datas->level->sensor[i]);
//                 // }
//                 for (int i = 0; i < 1; i++)
//                 {
//                     // sensor_datas->level->sensor[i] = fixed_temp[i];
//                     sensor_datas->level->sensor[i] = 2 + '0';
//                     sensor_datas->level->sensor[i + 1] = 5 + '0';
//                     // printf("%c%c ", sensor_datas->level->sensor[i], sensor_datas->level->sensor[i + 1]);
//                 }
//                 // sensor_datas->level->sensor[0] = 2 + '0';
//                 // sensor_datas->level->sensor[1] = 5 + '0';
//             }
//             int rand_num = (rand() % 100) + 1;

//             switch (sensor_datas->type)
//             {
//             case 'N':

//                 sensor_datas->level->sensor[0] = 2 + '0';
//                 sensor_datas->level->sensor[1] = 3 + '0';
//                 if (!temp_high)
//                 {
//                     if ((rand() % 100 + 1) <= 1)
//                     {
//                         // 0-10
//                         temp_high = true;
//                         break;
//                     }
//                     else
//                     // 11-100
//                     {
//                         // 11-40
//                         if ((rand() % 100 + 1) < 40)
//                         {
//                             sensor_datas->level->sensor[0] = 2 + '0';
//                             sensor_datas->level->sensor[1] = 5 + '0';
//                             break;
//                         }
//                         // else
//                         // 41-60
//                         else if ((rand() % 100 + 1) < 60)
//                         {
//                             sensor_datas->level->sensor[0] = 2 + '0';
//                             sensor_datas->level->sensor[1] = 2 + '0';
//                             break;
//                         }
//                         // 60-
//                         else if ((rand() % 100 + 1) < 80)
//                         {
//                             sensor_datas->level->sensor[0] = 2 + '0';
//                             sensor_datas->level->sensor[1] = 4 + '0';
//                             break;
//                         }
//                         // else
//                         else if ((rand() % 100 + 1) <= 100)
//                         {
//                             sensor_datas->level->sensor[0] = 2 + '0';
//                             sensor_datas->level->sensor[1] = 1 + '0';
//                             break;
//                         }
//                     }
//                 }
//                 if (temp_high)
//                 {
//                     high_temp_count++;
//                     sensor_datas->level->sensor[0] = 6 + '0';
//                     sensor_datas->level->sensor[1] = 1 + '0';
//                     if (high_temp_count > 10000)
//                     {
//                         temp_high = false;
//                         break;
//                     }
//                     else
//                     {
//                         if ((rand() % 100 + 1) < 50)
//                         {
//                             sensor_datas->level->sensor[0] = 6 + '0';
//                             sensor_datas->level->sensor[1] = 2 + '0';
//                             break;
//                         }
//                         else
//                         {
//                             sensor_datas->level->sensor[0] = 6 + '0';
//                             sensor_datas->level->sensor[1] = 0 + '0';
//                             break;
//                         }
//                     }
//                 }
//                 break;
//             case 'R':
//                 if (rand_num >= 60)
//                 {
//                     int c = (atoi(oldtemp)) + 4;
//                     char *raise_temp;
//                     raise_temp = toArray(c);
//                     for (int i = 0; i < 1; i++)
//                     {
//                         sensor_datas->level->sensor[i] = raise_temp[i];
//                         sensor_datas->level->sensor[i + 1] = raise_temp[i + 1];
//                         // printf("%c%c ", sensor_datas->level->sensor[i], sensor_datas->level->sensor[i + 1]);

//                         // printf("%d ", sensor_datas->level->sensor[i]);
//                     }
//                     // level->temp_sensor = oldtemp + 4;
//                 }
//                 break;
//             case 'F':
//                 // fixed = 60;
//                 // // char *fixed_temp;
//                 // fixed_temp = toArray(fixed);

//                 // sensor_datas->level->sensor[0] = 6 + '0';
//                 // sensor_datas->level->sensor[1] = 0 + '0';

//                 for (int i = 0; i < 1; i++)
//                 {
//                     // sensor_datas->level->sensor[i] = fixed_temp[i];
//                     sensor_datas->level->sensor[i] = 6 + '0';
//                     sensor_datas->level->sensor[i + 1] = 0 + '0';
//                     // printf("%c%c ", sensor_datas->level->sensor[i], sensor_datas->level->sensor[i + 1]);
//                 }
//                 break;
//             }
//         }
//         msleep(500);
//     }
//     return NULL;
// }