#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <semaphore.h>
#include <stdlib.h>

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
        boom_gate->status = BG_RAISING;
        pthread_cond_broadcast(&boom_gate->cond);
    }
    else if (update_status == BG_LOWERING)
    {
        boom_gate->status = BG_LOWERING;
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
        if (pthread_mutex_lock(&LPR->mutex) != 0)
        {
            perror("pthread_mutex_lock(&LPR->mutex)");
            exit(1);
        };
        // Wait for entrance LPR to clear
        while (LPR->plate[0] == '\0')
        {
            pthread_cond_wait(&LPR->cond, &LPR->mutex);
        }
        if (pthread_mutex_unlock(&LPR->mutex) != 0)
        {
            perror("pthread_mutex_lock(&LPR->mutex)");
            exit(1);
        };


        item_t *permitted_car = htab_find(hashtable, LPR->plate);
        // Car is permitted
        if (permitted_car != (item_t *)NULL)
        {
            pthread_mutex_lock(&capacity->mutex);
            int directed_lvl = get_empty_spot(capacity);
            pthread_mutex_unlock(&capacity->mutex);
            permitted_car->directed_lvl = directed_lvl;
            if (directed_lvl != 0)
            {
                pthread_mutex_lock(&info_sign->mutex);
                info_sign->display = directed_lvl + '0';
                pthread_mutex_unlock(&info_sign->mutex);

                pthread_mutex_lock(&boom_gate->mutex);
                control_boom_gate(boom_gate, BG_RAISING);
                // Wait for boom gate open
                while (!(boom_gate->status == BG_OPENED))
                {
                    pthread_cond_wait(&boom_gate->cond, &boom_gate->mutex);
                }
                pthread_mutex_unlock(&boom_gate->mutex);

                if (boom_gate->status == BG_OPENED)
                {
                    start_time(hashtable, LPR->plate);
                    permitted_car->entered = true;
                    // Lower after 20ms
                    msleep(20 * TIME_MULTIPLIER); 
                    pthread_mutex_lock(&boom_gate->mutex);
                    boom_gate->status = BG_LOWERING;
                    pthread_mutex_unlock(&boom_gate->mutex);
                    pthread_cond_broadcast(&boom_gate->cond);
                }
                pthread_mutex_lock(&boom_gate->mutex);
                while (!(boom_gate->status == BG_CLOSED))
                {
                    pthread_cond_wait(&boom_gate->cond, &boom_gate->mutex);
                }
                pthread_mutex_unlock(&boom_gate->mutex);
            }
            // Car park full
            else
            {
                pthread_mutex_lock(&info_sign->mutex);
                info_sign->display = 'F';
                pthread_cond_broadcast(&info_sign->cond);
                pthread_mutex_unlock(&info_sign->mutex);
            }
        }
        // Car not permitted
        else
        {
            pthread_mutex_lock(&info_sign->mutex);
            info_sign->display = 'X';
            pthread_cond_broadcast(&info_sign->cond);
            pthread_mutex_unlock(&info_sign->mutex);
        }

        // Clear LPR
        pthread_mutex_lock(&LPR->mutex);
        for (int i = 0; i < 6; i++)
        {
            LPR->plate[i] = '\0';
        }
        pthread_cond_broadcast(&LPR->cond);
        pthread_mutex_unlock(&LPR->mutex);

        // Clear info sign
        pthread_mutex_lock(&info_sign->mutex);
        info_sign->display = '\0';
        pthread_mutex_unlock(&info_sign->mutex);
    }
}

void *monitor_exit(void *data)
{
    monitor_exit_t *monitor_exit_data = (monitor_exit_t *)data;
    exit_t *exit = monitor_exit_data->exit;
    boom_gate_t *boom_gate = &exit->boom_gate;
    LPR_t *exit_LPR = &exit->lpr;
    htab_t *hashtable = monitor_exit_data->hashtable;
    level_t *level = monitor_exit_data->level;
    double *revenue = monitor_exit_data->revenue;

    while (true)
    {
        msleep(2 * TIME_MULTIPLIER);
        pthread_mutex_lock(&exit_LPR->mutex);
        while (exit_LPR->plate[0] == '\0')
        {
            pthread_cond_wait(&exit_LPR->cond, &exit_LPR->mutex);
        }
        pthread_mutex_unlock(&exit_LPR->mutex);

        // Mock boom gate open if fire
        if (level->alarm == '1')
        {
            continue;
        }
        pthread_mutex_lock(&boom_gate->mutex);
        control_boom_gate(boom_gate, BG_RAISING);

        // Wait boom gate open
        while (!(boom_gate->status == BG_OPENED))
        {
            pthread_cond_wait(&boom_gate->cond, &boom_gate->mutex);
        }
        pthread_mutex_unlock(&boom_gate->mutex);

        // Auto lowering boom gate
        if (boom_gate->status == BG_OPENED)
        {
            // Calc bill
            pthread_mutex_lock(&hashtable->mutex);
            item_t *car = htab_find(hashtable, exit_LPR->plate);
            calc_bill(hashtable, exit_LPR->plate, revenue);
            
            // Reset car for re-entry
            car->entry_time = 0;
            car->actual_lvl = 0;
            car->directed_lvl = 0;
            pthread_mutex_unlock(&hashtable->mutex);
            
            // Lower boom gate after 20ms
            msleep(20 * TIME_MULTIPLIER); 
            pthread_mutex_lock(&boom_gate->mutex);
            boom_gate->status = BG_LOWERING;
            pthread_mutex_unlock(&boom_gate->mutex);
            pthread_cond_broadcast(&boom_gate->cond);
        }
        pthread_mutex_lock(&boom_gate->mutex);
        
        // Wait boom gate closed
        while (!(boom_gate->status == BG_CLOSED))
        {
            pthread_cond_wait(&boom_gate->cond, &boom_gate->mutex);
        }
        pthread_mutex_unlock(&boom_gate->mutex);

        // Clear exit LPR
        pthread_mutex_lock(&exit_LPR->mutex);
        for (int i = 0; i < 6; i++)
        {
            exit_LPR->plate[i] = '\0';
        }
        pthread_cond_broadcast(&exit_LPR->cond);
        pthread_mutex_unlock(&exit_LPR->mutex);
    }
}

void *monitor_lpr(void *data)
{
    level_lpr_data_t *manager_lpr_data = (level_lpr_data_t *)data;
    LPR_t *lpr = manager_lpr_data->lpr;
    capacity_t *capacity = manager_lpr_data->capacity;
    htab_t *hashtable = manager_lpr_data->hashtable;
    int level = manager_lpr_data->level;

    while (true)
    {
        pthread_mutex_lock(&lpr->mutex);
        while (lpr->plate[0] == '\0')
        {
            pthread_cond_wait(&lpr->cond, &lpr->mutex);
        }
        pthread_mutex_unlock(&lpr->mutex);

        item_t *car = htab_find(hashtable, lpr->plate);

        // First time car triggers LPR going in
        if (car->entered == true)
        {
            car->actual_lvl = level;
            pthread_mutex_lock(&capacity->mutex);
            // Increase level capacity
            set_capacity(capacity, level);
            pthread_mutex_unlock(&capacity->mutex);
            car->entered = false;
        }
        // Second time car triggers LPR going out
        else
        {
            pthread_mutex_lock(&capacity->mutex);
            // Decrease level capacity
            free_carpark_space(capacity, level);
            pthread_mutex_unlock(&capacity->mutex);
        }

        // Clear LPR
        pthread_mutex_lock(&lpr->mutex);
        msleep(2 * TIME_MULTIPLIER);
        for (int i = 0; i < 6; i++)
        {
            lpr->plate[i] = '\0';
        }
        pthread_cond_broadcast(&lpr->cond);
        pthread_mutex_unlock(&lpr->mutex);
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