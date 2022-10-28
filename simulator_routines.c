#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "carpark_details.h"
#include "shared_memory.h"
#include "queue.h"
#include "hashtable.h"
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

    // pthread_mutex_lock(&initialisation_mutex);
    // ready_threads++;
    // pthread_cond_broadcast(&initialisation_cond);
    // pthread_mutex_unlock(&initialisation_mutex);

    while (true)
    {
        // printf("\n\tManage entrance loop ran\n");

        if (pthread_mutex_lock(&entrance_LPR->mutex) != 0)
        {
            perror("pthread_mutex_lock(&LPR->mutex)");
            exit(1);
        };
        while (entrance_LPR->plate[0] != NULL)
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

void *handle_level_lpr(void *data)
{
    // if lpr->plate != Null
    // signal to manager to read the plate in lpr

    // clear lpr->plate ???
}

void *car_logic(void *data)
{
    // printf("Car logic started\n");
    level_lpr_data_t *level_lpr_data = (level_lpr_data_t *)data;
    car_t *car = level_lpr_data->car;
    LPR_t *lpr = level_lpr_data->lpr;

    msleep(10 * TIME_MULTIPLIER); // Takes 10ms to go to its parking spot

    // printf("Attemp to change level LPR\n");
    if (pthread_mutex_lock(&lpr->mutex) != 0)
    {
        perror("pthread_mutex_lock(&level_lpr->mutex)");
        exit(1);
    };
    for (int i = 0; i < 6; i++)
    {
        lpr->plate[i] = car->plate[i];
    }
    // printf("Broadcast LPR\n");
    if (pthread_cond_broadcast(&lpr->cond) != 0)
    {
        perror("pthread_cond_broadcast(&level_lpr->mutex)");
        exit(1);
    };
    if (pthread_mutex_unlock(&lpr->mutex) != 0)
    {
        perror("pthread_mutex_unlock(&level_lpr->mutex)");
        exit(1);
    };
    // Trigger level LPR here prob with mutex locks and conds
    // pthread_mutex_lock
    // Then the manager sees the level LPR has been triggerd

    // Then increases capacity by 1 at that level and start billing

    // printf("***********************************\n");
    // printf("level lpr reads: %s\n", lpr->plate);
    // printf("The car: %s is parked at: %d\n", car->plate, car->directed_lvl);
    // printf("***********************************\n");

    // printf("The car: %s triggered level: %d LPR\n", car->plate, car->directed_lvl);

    msleep(1 * TIME_MULTIPLIER); // Park for 10-10000ms
    // Trigger level LPR again when exiting

    // Queue up at exit

    // printf("==================================\n");
    // printf("The car: %s has now left\n", car->plate);
    // printf("==================================\n");
    // pthread_exit(NULL);
}

void *handle_entrance_boomgate(void *data)
{
    entrance_data_shm_t *entrance_data_shm = (entrance_data_shm_t *)data;
    shared_memory_t *shm = entrance_data_shm->shm;
    entrance_t *entrance = entrance_data_shm->entrance;
    boom_gate_t *boom_gate = &entrance->boom_gate;
    LPR_t *lpr = &entrance->lpr;
    info_sign_t *sign = &entrance->info_sign;

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
            car_t *car_data = malloc(sizeof(car_t));
            char *plate_copy = malloc(sizeof(char) * (strlen(lpr->plate) + 1));
            strcpy(plate_copy, lpr->plate);
            car_data->plate = plate_copy;
            int level = (sign->display - '0') - 1;
            car_data->directed_lvl = level;
            level_lpr_data->car = car_data;
            LPR_t *directed_level_LPR;
            get_lpr(shm, level, &directed_level_LPR);
            level_lpr_data->lpr = directed_level_LPR;
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

// void assign_cars(void *arg)
// {
//     // Car logic stuff
//     pthread_t car;
//     car_t *car_data = malloc(sizeof(car_t));
//     char *plate_copy = malloc(sizeof(char) * (strlen(lpr->plate) + 1));
//     strcpy(plate_copy, lpr->plate);
//     car_data->plate = plate_copy;
//     int level = sign->display - '0';
//     car_data->directed_lvl = level;
//     pthread_create(&car, NULL, car_logic, (void *)car_data);
// }

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

        // msleep(rand_car_gen_time * TIME_MULTIPLIER);
        msleep(100);
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
        strcpy(plate_string, six_d_plate);
        pthread_mutex_lock(&entrance_datas[rand_entrance].queue_mutex);
        if (htab_find(hashtable, plate_string) == NULL)
        {
            htab_add(hashtable, plate_string);
            enqueue(entrance_datas[rand_entrance].entrance_queue, plate_string);
        }

        pthread_cond_broadcast(&entrance_datas[rand_entrance].cond);

        pthread_mutex_unlock(&entrance_datas[rand_entrance].queue_mutex);

        // printf("%s queued to %p: queue #%d \n", six_d_plate, entrance_datas[rand_entrance].entrance_queue, rand_entrance);
        for (int i = 0; i < ENTRANCES; i++)
        {
            printf("Entrace %d: ", i);
            print_queue(entrance_datas[i].entrance_queue);
        }
        printf("\n");
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
