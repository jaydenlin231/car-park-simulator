// #include <stdbool.h>
// #include <semaphore.h>
// #include "carpark_details.h"
// #include "hashtable.h"
// #include "manager_routines.h"
// #include "utility.h"
// #include "capacity.h"

// void *control_boom_gate(boom_gate_t *boom_gate, char update_status)
// {
//     if (update_status == BG_RAISING)
//     {
//         pthread_mutex_lock(&boom_gate->mutex);
//         printf("Instruct Boom Gate %p Raising\n", boom_gate);
//         boom_gate->status = BG_RAISING;
//         pthread_mutex_unlock(&boom_gate->mutex);
//         pthread_cond_broadcast(&boom_gate->cond);
//     }
//     else if (update_status == BG_LOWERING)
//     {
//         printf("Instruct Boom Gate Lowering\n");
//         pthread_mutex_lock(&boom_gate->mutex);
//         boom_gate->status = BG_LOWERING;
//         pthread_mutex_unlock(&boom_gate->mutex);
//         pthread_cond_broadcast(&boom_gate->cond);
//     }
//     return NULL;
// }

// void *monitor_entrance(void *data)
// {
//     // pthread_mutex_lock(&initialisation_mutex);
//     // ready_threads++;
//     // pthread_cond_broadcast(&initialisation_cond);
//     // pthread_mutex_unlock(&initialisation_mutex);

//     monitor_entrance_t *monitor_entrance = (monitor_entrance_t *)data;
//     entrance_t *entrance = monitor_entrance->entrance;
//     boom_gate_t *boom_gate = &entrance->boom_gate;
//     info_sign_t *info_sign = &entrance->info_sign;
//     LPR_t *LPR = &entrance->lpr;
//     htab_t *hashtable = monitor_entrance->hashtable;
//     capacity_t *capacity = monitor_entrance->capacity;

//     while (true)
//     {
//         if (pthread_mutex_lock(&LPR->mutex) != 0)
//         {
//             perror("pthread_mutex_lock(&LPR->mutex)");
//             exit(1);
//         };

//         while (LPR->plate[0] == NULL)
//         {
//             // printf("\t\tCond Wait LPR not NULL, currently: %s\n", LPR->plate);
//             pthread_cond_wait(&LPR->cond, &LPR->mutex);
//         }

//         printf("%s is at the LPR\n", LPR->plate);
//         item_t *permitted_car = htab_find(hashtable, LPR->plate);
//         if (permitted_car != NULL)
//         {
//             printf("%s is in the permitted list\n", permitted_car->key);
//             msleep(1 * TIME_MULITIPLIER);
//             int directed_lvl = get_set_empty_spot(capacity);
//             permitted_car->directed_lvl = directed_lvl;
//             permitted_car->actual_lvl = directed_lvl; // For now actual = directed until we add randomness
//             if (directed_lvl != NULL)
//             {
//                 pthread_mutex_lock(&info_sign->mutex);
//                 info_sign->display = directed_lvl + '0';
//                 printf("Info sign says: %c\n", info_sign->display);
//                 pthread_mutex_unlock(&info_sign->mutex);
//                 printf("Directed to level: %d\n", directed_lvl);
//                 print_capacity(capacity);
//                 control_boom_gate(boom_gate, BG_RAISING);

//                 pthread_mutex_lock(&boom_gate->mutex);
//                 while (!(boom_gate->status == BG_OPENED))
//                 {
//                     printf("\tBoom Gate %p Cond Wait test BG_OPENED. Current Status: %c.\n", boom_gate, boom_gate->status);
//                     if (pthread_cond_wait(&boom_gate->cond, &boom_gate->mutex))
//                     {
//                         perror("WTF");
//                         exit(1);
//                     };
//                 }
//                 pthread_mutex_unlock(&boom_gate->mutex);

//                 if (boom_gate->status == BG_OPENED)
//                 {
//                     printf("Lowering Boom Gate %p...\n", boom_gate);
//                     printf("Waiting 10 ms\n");
//                     msleep(100 * TIME_MULITIPLIER);
//                     pthread_mutex_lock(&boom_gate->mutex);
//                     boom_gate->status = BG_LOWERING;
//                     printf("Boom Gate %p Lowered\n", boom_gate);
//                     pthread_mutex_unlock(&boom_gate->mutex);
//                     pthread_cond_broadcast(&boom_gate->cond);
//                 }
//             }
//             else
//             {
//                 printf("Carpark FULL\n");
//             }
//         }

//         if (pthread_mutex_unlock(&LPR->mutex) != 0)
//         {
//             perror("pthread_mutex_unlock(&LPR->mutex)");
//             exit(1);
//         };

//         if (pthread_mutex_lock(&LPR->mutex) != 0)
//         {
//             perror("pthread_mutex_unlock(&LPR->mutex)");
//             exit(1);
//         };
//         // Clear LPR
//         for (int i = 0; i < 6; i++)
//         {
//             LPR->plate[i] = NULL;
//         }
//         pthread_cond_broadcast(&LPR->cond);
//         pthread_mutex_unlock(&LPR->mutex);
//     }

//     // Wait for shm->entrances->LPR->plate to be !NULL

//     // htable_find(&hashtable, shm->entrances->LPR->plate);
//     // if (htable_find == NUll) return
//     // else control_boomgate(boomgate, Raise)
// }

// void *handle_boom_gate(void *data)
// {
//     boom_gate_t *boom_gate = (boom_gate_t *)data;
//     printf("Created Boom Gate %p Thread\n", boom_gate);

//     // pthread_mutex_lock(&initialisation_mutex);
//     // ready_threads++;
//     // pthread_mutex_unlock(&initialisation_mutex);
//     // pthread_cond_signal(&initialisation_cond);

//     printf("Begin Boom Gate %p Thread\n", boom_gate);
//     int test = 0;
//     while (true)
//     {
//         printf("\tBoom Gate %p Monitor Loop ran %d times.\n", boom_gate, ++test);

//         pthread_mutex_lock(&boom_gate->mutex);
//         while (!(boom_gate->status == BG_OPENED))
//         {
//             // printf("\tBoom Gate %p Cond Wait BG_OPENED. Current Status: %c.\n", boom_gate, boom_gate->status);
//             pthread_cond_wait(&boom_gate->cond, &boom_gate->mutex);
//         }
//         pthread_mutex_unlock(&boom_gate->mutex);

//         if (boom_gate->status == BG_OPENED)
//         {
//             printf("Lowering Boom Gate %p...\n", boom_gate);
//             printf("Waiting 10 ms\n");
//             msleep(1 * TIME_MULITIPLIER);
//             pthread_mutex_lock(&boom_gate->mutex);
//             boom_gate->status = BG_LOWERING;
//             printf("Boom Gate %p Lowered\n", boom_gate);
//             pthread_mutex_unlock(&boom_gate->mutex);
//             pthread_cond_broadcast(&boom_gate->cond);
//         }
//         // else if (boom_gate->status == BG_CLOSED)
//         // {
//         //     printf("Lowering Boom Gate %p...\n", boom_gate);
//         //     printf("Waiting 10 ms\n");
//         //     msleep(2 * TIME_MULITIPLIER);
//         //     pthread_mutex_lock(&boom_gate->mutex);
//         //     boom_gate->status = BG_RAISING;
//         //     pthread_cond_broadcast(&boom_gate->cond);
//         //     printf("Boom Gate %p Lowered\n", boom_gate);
//         //     pthread_mutex_unlock(&boom_gate->mutex);
//         // }
//     }
//     // Might not be needed
//     printf("Boom Gate Quit before broadcast %p...\n", boom_gate);
//     return NULL;
// }

// void *wait_sim_close(void *data)
// {
//     // shared_memory_t *shm = (shared_memory_t *)data;
//     sim_man_sem_t *sim_man_sem = (sim_man_sem_t *)data;
//     sem_t *simulation_ended_sem = sim_man_sem->simulation_ended_sem;
//     sem_t *manager_ended_sem = sim_man_sem->manager_ended_sem;
//     printf("Monitor Thread Waiting for simulation to close\n");
//     sem_wait(simulation_ended_sem);
//     printf("Monitor notified simulation closed\n");

//     sem_post(manager_ended_sem);
//     return NULL;
// }