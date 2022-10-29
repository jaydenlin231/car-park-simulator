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

#define SHM_NAME "/PARKING"
#define SHM_SZ sizeof(data_t)

#define SHM_EST_SEM_NAME "/SHM_ESTABLISHED"
#define SIM_READY_SEM_NAME "/SIM_READY"
#define SIM_END_SEM_NAME "/SIM_ENDED"
#define MAN_READY_SEM_NAME "/MAN_READY"
#define MAN_END_SEM_NAME "/MAN_ENDED"

#define PLATE_FILE "plates.txt"
#define MAX_LINE_LENGTH 7
#define MAX_IMPORTED_PLATES 100

static pthread_mutex_t initialisation_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t initialisation_cond = PTHREAD_COND_INITIALIZER;

static htab_t hashtable;
static capacity_t capacity;

double total_revenue = 0.00;

int main()
{
    srand(time(NULL));

    shared_memory_t shm;

    sem_t *shm_established_sem = sem_open(SHM_EST_SEM_NAME, 0);
    sem_t *simulation_ready_sem = sem_open(SIM_READY_SEM_NAME, 0);
    sem_t *manager_ready_sem = sem_open(MAN_READY_SEM_NAME, 0);
    // simulation_ended_sem = sem_open(SIM_END_SEM_NAME, 0);
    // manager_ended_sem = sem_open(MAN_END_SEM_NAME, 0);

    pthread_t monitor_sim_thread;

    // Initialise hash table from "plates.txt" file
    hashtable = import_htable(PLATE_FILE);

    // Initialise Capcity
    init_capacity(&capacity);
    // print_capacity(&capacity);

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

    printf("Init Entrance threads.\n");
    // pthread_t entrance_BG_threads[ENTRANCES];
    pthread_t entrance_threads[ENTRANCES];
    entrance_t *entrance;
    entrance_data_t entrance_data[ENTRANCES];
    for (size_t i = 0; i < ENTRANCES; i++)
    {
        get_entrance(&shm, i, &entrance);
        entrance_data[i].entrance = entrance;
        entrance_data[i].capacity = &capacity;
        entrance_data[i].hashtable = &hashtable;
        boom_gate_t *boom_gate = malloc(sizeof(boom_gate_t));
        boom_gate = &entrance->boom_gate;
        printf("Entrance %ld Boom Gate %p\n", i, &entrance->boom_gate);
        pthread_create(&entrance_threads[i], NULL, monitor_entrance, (void *)&entrance_data[i]);
    }

    pthread_t level_lpr[LEVELS];
    // LPR_t *lpr;
    level_t *level;
    manager_lpr_data_t level_data[LEVELS];
    for (size_t i = 0; i < LEVELS; i++)
    {
        // get_lpr(&shm, i, &lpr);
        get_level(&shm, i, &level);
        printf("level is at %p\n", level);
        level_data[i].lpr = &(level->lpr);
        level_data[i].hashtable = &hashtable;
        level_data[i].capacity = &capacity;
        level_data[i].level = i + 1;
        level_data[i].total_revenue = &total_revenue;
        pthread_mutex_init(&level_data[i].total_rev_mutex, NULL);
        pthread_create(&level_lpr[i], NULL, monitor_lpr, (void *)&level_data[i]);
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
        printf("\t Total Revenue: $%0.2lf", total_revenue);
        if (capacity.full)
        {
            printf("\033[1;31m");
            printf("\t\tCARPARK FULL");
            printf("\033[0;37m");
        }
        printf("\n");

        for (int i = 0; i < ENTRANCES; i++)
        {
            printf("Entrance: %d \t| License Plate Reader: %s\t| Boom Gate: %c\t| Sign: %c\n", i + 1, entrance_data[i].entrance->lpr.plate, entrance_data[i].entrance->boom_gate.status, entrance_data[i].entrance->info_sign.display);
        }
        printf("\n");

        for (int i = 0; i < LEVELS; i++)
        {
            printf("Level: %d \t| License Plate Reader: %s\t| Capacity: %d/%d\n", i + 1, level_data[i].lpr->plate, capacity.curr_capacity[i], NUM_SPOTS_LVL);
        }
        printf("\n");

        // for (int i = 0; i < EXITS; i++)
        // {
        //     printf("Exit: %d \t| License Plate Reader: %s\t| Boom Gate: %c\n", i + 1, | BOOM GATE STATE |, exit_lps_current[i]);
        // }

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
