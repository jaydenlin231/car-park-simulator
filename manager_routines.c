#include <stdbool.h>
#include <semaphore.h>
#include "carpark_details.h"
#include "hashtable.h"
#include "manager_routines.h"
#include "billing.h"
#include "utility.h"
#include "capacity.h"

#define SIM_END_SEM_NAME "/SIM_ENDED"
#define MAN_END_SEM_NAME "/MAN_ENDED"

void *control_boom_gate(boom_gate_t *boom_gate, char update_status)
{
    if (update_status == BG_RAISING)
    {
        // pthread_mutex_lock(&boom_gate->mutex);
        // printf("Instruct Boom Gate %p Raising\n", boom_gate);
        boom_gate->status = BG_RAISING;
        // pthread_mutex_unlock(&boom_gate->mutex);
        pthread_cond_broadcast(&boom_gate->cond);
    }
    else if (update_status == BG_LOWERING)
    {
        // printf("Instruct Boom Gate Lowering\n");
        // pthread_mutex_lock(&boom_gate->mutex);
        boom_gate->status = BG_LOWERING;
        // pthread_mutex_unlock(&boom_gate->mutex);
        pthread_cond_broadcast(&boom_gate->cond);
    }
    return NULL;
}

void *monitor_entrance(void *data)
{
    entrance_data_t *monitor_entrance_data = (entrance_data_t *)data;
    entrance_t *entrance = monitor_entrance_data->entrance;
    boom_gate_t *boom_gate = &entrance->boom_gate;
    info_sign_t *info_sign = &entrance->info_sign;
    LPR_t *LPR = &entrance->lpr;
    htab_t *hashtable = monitor_entrance_data->hashtable;
    capacity_t *capacity = monitor_entrance_data->capacity;

    while (true)
    {
        // printf("Using hashtable %p\n", hashtable);
        if (pthread_mutex_lock(&LPR->mutex) != 0)
        {
            perror("pthread_mutex_lock(&LPR->mutex)");
            exit(1);
        };
        while (LPR->plate[0] == NULL)
        {
            // printf("\t\tCond Wait LPR not NULL, currently: %s\n", LPR->plate);
            pthread_cond_wait(&LPR->cond, &LPR->mutex);
        }

        printf("%s is at the entrance LPR\n", LPR->plate);
        item_t *permitted_car = htab_find(hashtable, LPR->plate);
        if (permitted_car != NULL)
        {
            printf("%s is in the permitted list\n", permitted_car->key);
            pthread_mutex_lock(&capacity->mutex);
            int directed_lvl = get_empty_spot(capacity);
            pthread_mutex_unlock(&capacity->mutex);
            permitted_car->directed_lvl = directed_lvl;
            // permitted_car->actual_lvl = directed_lvl; // For now actual = directed until we add randomness
            if (directed_lvl != NULL)
            {
                pthread_mutex_lock(&info_sign->mutex);
                info_sign->display = directed_lvl + '0';
                printf("Info sign says: %c\n", info_sign->display);
                pthread_mutex_unlock(&info_sign->mutex);
                // printf("Directed to level: %d\n", directed_lvl);
                // print_capacity(capacity);

                pthread_mutex_lock(&boom_gate->mutex);
                control_boom_gate(boom_gate, BG_RAISING);
                while (!(boom_gate->status == BG_OPENED))
                {
                    // printf("\tBoom Gate %p Cond Wait BG_OPENED. Current Status: %c.\n", boom_gate, boom_gate->status);
                    pthread_cond_wait(&boom_gate->cond, &boom_gate->mutex);
                    // printf("\tBoom Gate %p After Cond Wait BG_OPENED. Current Status: %c.\n", boom_gate, boom_gate->status);
                }
                pthread_mutex_unlock(&boom_gate->mutex);

                if (boom_gate->status == BG_OPENED)
                {
                    // printf("Lowering Boom Gate %p...\n", boom_gate);
                    // printf("Currently Open for 20 ms\n"); // The car is travelling to its spot as soon as it opened
                    msleep(20 * TIME_MULTIPLIER); // Lower after 20ms
                    pthread_mutex_lock(&boom_gate->mutex);
                    boom_gate->status = BG_LOWERING;
                    // printf("Boom Gate %p Lowered\n", boom_gate);
                    pthread_mutex_unlock(&boom_gate->mutex);
                    pthread_cond_broadcast(&boom_gate->cond);
                }
                pthread_mutex_lock(&boom_gate->mutex);
                while (!(boom_gate->status == BG_CLOSED))
                {
                    // printf("\tBoom Gate %p Cond Wait BG_CLOSED. Current Status: %c.\n", boom_gate, boom_gate->status);
                    pthread_cond_wait(&boom_gate->cond, &boom_gate->mutex);
                    // printf("\tBoom Gate %p After Cond Wait BG_CLOSED. Current Status: %c.\n", boom_gate, boom_gate->status);
                }
                pthread_mutex_unlock(&boom_gate->mutex);
            }
            else
            {
                printf("Carpark FULL\n");
            }
        }

        if (pthread_mutex_unlock(&LPR->mutex) != 0)
        {
            perror("pthread_mutex_unlock(&LPR->mutex)");
            exit(1);
        };
        // Clear LPR
        for (int i = 0; i < 6; i++)
        {
            LPR->plate[i] = NULL;
        }
        pthread_cond_broadcast(&LPR->cond);
        pthread_mutex_unlock(&LPR->mutex);
    }
}

void *monitor_lpr(void *data)
{
    manager_lpr_data_t *manager_lpr_data = (manager_lpr_data_t *)data;
    LPR_t *level_lpr = manager_lpr_data->lpr;
    capacity_t *capacity = manager_lpr_data->capacity;
    htab_t *hashtable = manager_lpr_data->hashtable;
    int level = manager_lpr_data->level;

    while (true)
    {
        pthread_mutex_lock(&level_lpr->mutex);
        while (level_lpr->plate[0] == NULL)
        {
            // printf("\t\tCond Wait LPR not NULL, currently: %s\n", level_lpr->plate);
            pthread_cond_wait(&level_lpr->cond, &level_lpr->mutex);
        }
        pthread_mutex_lock(&level_lpr->mutex);
        printf("%s is at the level %d LPR\n", level_lpr->plate, level);
        item_t *car = htab_find(hashtable, level_lpr->plate);
        pthread_mutex_lock(&capacity->mutex);
        if (car->entry_time == 0)
        {
            printf("%s just parked!\n", car->key);
            car->actual_lvl = level;
            start_time(hashtable, car->key);
            set_capacity(capacity, level);
            print_capacity(capacity);
        }
        else
        {
            printf("%s left carpark!\n", car->key);
            calc_bill(hashtable, car->key);
            free_carpark_space(capacity, level);
            car->entry_time == 0;
            print_capacity(capacity);
        }
        pthread_mutex_unlock(&capacity->mutex);

        // Clear LPR
        for (int i = 0; i < 6; i++)
        {
            level_lpr->plate[i] = NULL;
        }

        pthread_cond_broadcast(&level_lpr->cond);
        pthread_mutex_unlock(&level_lpr->mutex);
    }
}

void *wait_sim_close(void *data)
{
    sem_t *simulation_ended_sem = sem_open(SIM_END_SEM_NAME, 0);
    sem_t *manager_ended_sem = sem_open(MAN_END_SEM_NAME, 0);
    printf("Monitor Thread Waiting for simulation to close\n");
    sem_wait(simulation_ended_sem);
    printf("Monitor notified simulation closed\n");
    sem_post(manager_ended_sem);
    return NULL;
}