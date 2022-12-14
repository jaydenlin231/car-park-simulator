#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "carpark_details.h"
#include "hashtable.h"
#include "shared_memory.h"
#include "capacity.h"
#include "utility.h"
#include "manager_routines.h"

#define SHM_NAME "PARKING"
#define SHM_SZ sizeof(data_t)

#define SHM_EST_SEM_NAME "/SHM_ESTABLISHED"
#define SIM_READY_SEM_NAME "/SIM_READY"
#define SIM_END_SEM_NAME "/SIM_ENDED"
#define MAN_READY_SEM_NAME "/MAN_READY"
#define MAN_END_SEM_NAME "/MAN_ENDED"

#define PLATE_FILE "plates.txt"
#define MAX_LINE_LENGTH 7
#define MAX_IMPORTED_PLATES 100

static htab_t hashtable;
static capacity_t capacity;

static double total_revenue = 0.00;

int main()
{
    // Randomise seed with current time
    srand(time(NULL));

    shared_memory_t shm;

    sem_t *shm_established_sem = sem_open(SHM_EST_SEM_NAME, 0);
    sem_t *simulation_ready_sem = sem_open(SIM_READY_SEM_NAME, 0);
    sem_t *manager_ready_sem = sem_open(MAN_READY_SEM_NAME, 0);


    pthread_t monitor_sim_thread;

    // Initialise hash table from "plates.txt" file
    hashtable = import_htable(PLATE_FILE);

    // Initialise Capcity
    init_capacity(&capacity);

    printf("Manager started.\n");
    if (get_shared_object(&shm))
    {
        sem_post(shm_established_sem);
        printf("Manager successfully connected to shm.");
    }
    else
    {
        perror("Shared memory connection failed.");
        return -1;
    }

    // Monitor Entrance Functionality
    pthread_t entrance_threads[ENTRANCES];
    entrance_t *entrance;
    entrance_data_t entrance_data[ENTRANCES];
    for (size_t i = 0; i < ENTRANCES; i++)
    {
        get_entrance(&shm, i, &entrance);
        entrance_data[i].entrance = entrance;
        entrance_data[i].capacity = &capacity;
        entrance_data[i].hashtable = &hashtable;
        pthread_create(&entrance_threads[i], NULL, monitor_entrance, (void *)&entrance_data[i]);
    }

    // Monitor Level LPR Functionality
    pthread_t level_lpr_threads[LEVELS];
    level_t *level;
    level_lpr_data_t level_data[LEVELS];
    for (size_t i = 0; i < LEVELS; i++)
    {
        get_level(&shm, i, &level);
        level_data[i].lpr = &(level->lpr);
        level_data[i].temp = (char *)&(level->sensor);
        level_data[i].alarm = &(level->alarm);
        level_data[i].hashtable = &hashtable;
        level_data[i].capacity = &capacity;
        level_data[i].level = i + 1;
        pthread_create(&level_lpr_threads[i], NULL, monitor_lpr, (void *)&level_data[i]);
    }

    // Monitor EXIT LPR Functionality
    pthread_t exit_threads[EXITS];
    exit_t *exit;
    monitor_exit_t exit_data[EXITS];
    for (size_t i = 0; i < EXITS; i++)
    {
        get_exit(&shm, i, &exit);
        exit_data[i].exit = exit;
        get_level(&shm, i, &level);
        exit_data[i].level = level;
        exit_data[i].hashtable = &hashtable;
        exit_data[i].revenue = &total_revenue;
        exit_data[i].exit_number = i + 1;
        pthread_mutex_init(&exit_data[i].revenue_mutex, NULL);
        pthread_create(&exit_threads[i], NULL, monitor_exit, (void *)&exit_data[i]);
    }

    printf("Waiting for simulation to ready.\n");
    sem_wait(simulation_ready_sem);
    printf("=================.\n");

    printf("Manager ready for simulation.\n");
    sem_post(manager_ready_sem);
    printf("=================.\n");
    printf("Start Monitor Thread.\n");
    pthread_create(&monitor_sim_thread, NULL, wait_sim_close, (void *)NULL);

    setvbuf(stdout, NULL, _IOFBF, 2000);
    do
    {
        system("clear");
        // Signs Display
        printf("\e[?25l");
        printf("Car Park Group 99\n\n");
        printf("Total ");
        print_capacity(&capacity);
        printf("\nTotal Revenue: ");
        printf("\033[0;32m");
        printf("$%0.2lf", total_revenue);
        printf("\033[0;37m");
        if (capacity.full)
        {
            printf("\033[1;31m");
            printf("\t\t\t\tCARPARK FULL");
            printf("\033[0;37m");
        }
        printf("\n\n");

        for (int i = 0; i < ENTRANCES; i++)
        {
            printf("Entrance: %d \t| License Plate Reader: %s\t| Boom Gate: %c\t| Sign: %c\n", i + 1, entrance_data[i].entrance->lpr.plate, entrance_data[i].entrance->boom_gate.status, entrance_data[i].entrance->info_sign.display);
        }
        printf("\n");

        for (int i = 0; i < LEVELS; i++)
        {
            printf("Level: %d \t| License Plate Reader: %s\t| Capacity: %d/%d\t| Temerature: %.2s C\t | Alarm: %.1s\n", i + 1, level_data[i].lpr->plate, capacity.curr_capacity[i], NUM_SPOTS_LVL, level_data[i].temp, level_data[i].alarm);
        }
        printf("\n");

        for (int i = 0; i < EXITS; i++)
        {
            printf("Exit: %d \t| License Plate Reader: %s\t| Boom Gate: %c\n", i + 1, exit_data[i].exit->lpr.plate, exit_data[i].exit->boom_gate.status);
        }

        printf("\n");
        fflush(stdout);
        msleep(10);

    } while (true);

    pthread_join(monitor_sim_thread, NULL);
    printf("=================.\n");
    printf("Joined Monitor Thread.\n");

    printf("=================.\n");
    printf("Joined All Thread.\n");

    printf("Manager Exited.\n");

    return 0;
}
