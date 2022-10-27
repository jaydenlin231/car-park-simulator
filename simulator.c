#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>
#include <string.h>
#include <inttypes.h>

#include "carpark_details.h"
#include "shared_memory.h"
#include "queue.h"
#include "hashtable.h"
#include "utility.h"
#include "simulator_routines.h"

#define SHM_NAME "/PARKING"
#define SHM_SZ sizeof(data_t)

#define SEM_SHARED 1

#define SHM_EST_SEM_NAME "/SHM_ESTABLISHED"
#define SIM_READY_SEM_NAME "/SIM_READY"
#define SIM_END_SEM_NAME "/SIM_ENDED"
#define MAN_READY_SEM_NAME "/MAN_READY"
#define MAN_END_SEM_NAME "/MAN_ENDED"

sem_t *manager_ended_sem;

int main()
{
    shared_memory_t shm;
    if (sem_unlink(SHM_EST_SEM_NAME) != 0)
    {
        perror("sem_unlink(SHM_EST_SEM_NAME) failed");
    }
    if (sem_unlink(SIM_READY_SEM_NAME) != 0)
    {
        perror("shm_unlink(SIM_READY_SEM_NAME) failed");
    }
    if (sem_unlink(SIM_END_SEM_NAME) != 0)
    {
        perror("shm_unlink(SIM_END_SEM_NAME) failed");
    }
    if (sem_unlink(MAN_READY_SEM_NAME) != 0)
    {
        perror("shm_unlink(SIM_END_SEM_NAME) failed");
    }
    if (sem_unlink(MAN_END_SEM_NAME) != 0)
    {
        perror("shm_unlink(SIM_END_SEM_NAME) failed");
    }

    sem_t *shm_established_sem = sem_open(SHM_EST_SEM_NAME, O_CREAT, 0644, 0);
    sem_t *simulation_ready_sem = sem_open(SIM_READY_SEM_NAME, O_CREAT, 0644, 0);
    sem_t *simulation_ended_sem = sem_open(SIM_END_SEM_NAME, O_CREAT, 0644, 0);
    sem_t *manager_ready_sem = sem_open(MAN_READY_SEM_NAME, O_CREAT, 0644, 0);
    manager_ended_sem = sem_open(MAN_END_SEM_NAME, O_CREAT, 0644, 0);

    printf("Attempt to create shared memory.\n");

    if (create_shared_object(&shm))
    {
        printf("Created shared memory.\n");
        printf("Init shared data.\n");
        init_shared_memory_data(&shm);
        printf("Finished init shared data.\n");
    }
    else
    {
        printf("Shared memory creation failed.\n");
        return -1;
    }

    printf("Waiting for Manager to connect to shm.\n");
    sem_wait(shm_established_sem);
    printf("Manager connected to shm.\n");

    init_entrance_data(&shm);
    printf("Finished init entrance data\n");

    pthread_t car_generation_thread;
    pthread_t monitor_sim_thread;

    pthread_t entrance_threads[ENTRANCES];
    pthread_t entrance_queue_threads[ENTRANCES];
    // pthread_t exit_threads[EXITS];
    // pthread_t level_threads[LEVELS];

    queue_t *entrance_queues[ENTRANCES];
    // sh_entrance_data_t *sh_entrance_data = malloc(sizeof(sh_entrance_data_t));

    entrance_data_t entrance_datas[ENTRANCES];

    printf("Init Entrance threads.\n");

    entrance_t *entrance;
    for (int i = 0; i < ENTRANCES; i++)
    {
        get_entrance(&shm, i, &entrance);
        entrance_datas[i].entrance = entrance;
        entrance_datas[i].entrance_queue = create_queue();
        pthread_mutex_init(&entrance_datas[i].queue_mutex, NULL);
        pthread_cond_init(&entrance_datas[i].cond, NULL);
        sem_init(&entrance_datas[i].entrance_LPR_free, SEM_SHARED, 1);

        // boom_gate_t *boom_gate = &entrance->boom_gate;
        // pthread_create(&entrance_threads[i], NULL, handle_boom_gate, (void *)boom_gate);
        pthread_create(&entrance_threads[i], NULL, handle_boom_gate, (void *)entrance);
        pthread_create(&entrance_queue_threads[i], NULL, handle_entrance_queue, (void *)&entrance_datas[i]);
    }
    // // exit_t *exit;
    // for (int i = 0; i < EXITS; i++) {
    //     int* data = malloc(sizeof(int));
    //     *data = i;
    //     pthread_create(&exit_threads[i], NULL, test_thread, (void *)data);
    // }
    // // level_t *level;
    // for (int i = 0; i < LEVELS; i++) {
    //     int* data = malloc(sizeof(int));
    //     *data = i;
    //     pthread_create(&level_threads[i], NULL, test_thread, (void *)data);
    // }

    printf("=================.\n");
    printf("Start Monitor Thread.\n");
    pthread_create(&monitor_sim_thread, NULL, wait_manager_close, (void *)manager_ended_sem);

    sem_post(simulation_ready_sem);
    printf("Simulation ready to start\n");
    printf("Waiting for manager ready\n");

    sem_wait(manager_ready_sem);

    printf("=================.\n");
    printf("Start Car Thread.\n");
    pthread_create(&car_generation_thread, NULL, generate_cars, (void *)&entrance_datas);

    pthread_join(car_generation_thread, NULL);
    printf("=================.\n");
    printf("Joined Car Thread.\n");

    sem_post(simulation_ended_sem);

    pthread_join(monitor_sim_thread, NULL);
    printf("=================.\n");
    printf("Manager ended.\n");

    // Might need this?
    // clean_shared_memory_data(&shm);
    // printf("clean_shared_memory_data done.\n");
    destroy_shared_object(&shm);
    printf("Destroyed shm.\n");

    printf("=================.\n");
    printf("Simulator Exited.\n");
    return 0;
}