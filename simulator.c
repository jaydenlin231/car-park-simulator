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



#include "carpark_types.h"
#include "shared_memory.h"

#define SHM_NAME "/PARKING"
#define SHM_SZ sizeof(data_t)

#define SHM_EST_SEM_NAME "/SHM_ESTABLISHED"
#define SIM_READY_SEM_NAME "/SIM_READY"
#define SIM_END_SEM_NAME "/SIM_ENDED"
#define MAN_END_SEM_NAME "/MAN_ENDED"


// TODO: Remove testing stub
#define NUM_OF_CARS 5

pthread_mutex_t initialisation_mutex;
pthread_cond_t initialisation_cond;

// TODO: Remove testing stub
int ready_threads = 0;
bool quit = false;

// TODO: Remove testing stub
void *test_thread(void *data) {
    pthread_mutex_lock(&initialisation_mutex);
    int *test = (int*) data;
    printf("Thread %d Received data: %d.\n", *test, *test);
    free(test);
    ready_threads++;
    pthread_cond_signal(&initialisation_cond);
    pthread_mutex_unlock(&initialisation_mutex);
    return NULL;
}

void *control_boom_gate(boom_gate_t *boom_gate, char update_status) {
    // printf("Instruct Boom Gate %p Before lock\n", boom_gate);
    // if(pthread_mutex_lock(&boom_gate->mutex)!= 0){
    //     perror("pthread_mutex_lock");
    // }
    // printf("Instruct Boom Gate %p After lock\n", boom_gate);

    if(update_status == BG_RAISING) {
            printf("Instruct Boom Gate %p Before lock\n", boom_gate);
            pthread_mutex_lock(&boom_gate->mutex);
            printf("Instruct Boom Gate %p Raising\n", boom_gate);
            boom_gate->status = BG_RAISING;
            pthread_mutex_unlock(&boom_gate->mutex);
            pthread_cond_signal(&boom_gate->cond);

        } else if(update_status == BG_LOWERING) {
            printf("Instruct Boom Gate Lowering\n");
            pthread_mutex_lock(&boom_gate->mutex);
            boom_gate->status = BG_LOWERING;
            pthread_mutex_unlock(&boom_gate->mutex);
            pthread_cond_signal(&boom_gate->cond);
        }
    // printf("Instruct Boom Gate %p Before unlock\n", boom_gate);
    // pthread_mutex_unlock(&boom_gate->mutex);
    // printf("Instruct Boom Gate %p After unlock\n", boom_gate);
    return NULL;

}


void *handle_boom_gate(void *data) {
    boom_gate_t *boom_gate = (boom_gate_t *)data;


    
    printf("Created Boom Gate %p Thread\n", boom_gate);

    if(pthread_mutex_lock(&initialisation_mutex) != 0){
        perror("init pthread_mutex_lock ");
        exit(1);
    }
    ready_threads++;
    pthread_cond_broadcast(&initialisation_cond);
    pthread_mutex_unlock(&initialisation_mutex);
    // int num = 0;
    printf("Started Boom Gate %p Thread\n", boom_gate);
    int test = 0;
    while (true) {
        printf("\tBoom Gate %p Loop ran %d times.\n",boom_gate, ++test);
        printf("\tBoom Gate %p Stauts: %c \n", boom_gate, boom_gate->status);
        printf("\tBoom Gate %p Before lock \n", boom_gate);
        if(pthread_mutex_lock(&boom_gate->mutex) != 0){
            perror("pthread_mutex_lock");
            exit(1);
        }
        printf("\tBoom Gate %p After lock \n", boom_gate);
        
        while (boom_gate->status != BG_RAISING && boom_gate->status != BG_LOWERING && boom_gate->status!= BG_QUIT) {
            printf("\tBoom Gate %p Beofre cond wait \n", boom_gate);
            if(pthread_cond_wait(&boom_gate->cond, &boom_gate->mutex) != 0){
                printf("\t\tBoom Gate %p Stauts: %c \n", boom_gate, boom_gate->status);
                printf("\t\tBoom Gate %p Cond wait Mutex addr:%p Cond addr:%p\n", boom_gate, &boom_gate->mutex, &boom_gate->cond);
                perror("\t\tBoom Gate pthread_cond_wait");
                exit(1);
            }
        }
        pthread_mutex_unlock(&boom_gate->mutex);
        if(boom_gate->status == BG_QUIT){
            printf("Boom Gate Quit from waiting %p...\n", boom_gate);
            return NULL;
        }

        printf("Boom Gate %p Stauts: %c \n", boom_gate, boom_gate->status);
        if(boom_gate->status == BG_RAISING) {
            printf("Raising Boom Gate %p...\n", boom_gate);
            printf("Waiting 10 ms\n");
            sleep(2);
            pthread_mutex_lock(&boom_gate->mutex);
            boom_gate->status = BG_OPENED;
            printf("Boom Gate %p Opened\n", boom_gate);
            pthread_cond_signal(&boom_gate->cond);
            pthread_mutex_unlock(&boom_gate->mutex);
        } else if(boom_gate->status == BG_LOWERING) {
            printf("Lowering Boom Gate %p...\n", boom_gate);
            printf("Waiting 10 ms\n");
            sleep(2);
            pthread_mutex_lock(&boom_gate->mutex);
            boom_gate->status = BG_CLOSED;
            printf("Boom Gate %p Closed\n", boom_gate);
            pthread_cond_signal(&boom_gate->cond);
            pthread_mutex_unlock(&boom_gate->mutex);
        } 
        // pthread_mutex_unlock(&boom_gate->mutex);
        // printf("Boom Gate %p After unlock \n", boom_gate);
        // printf("Quit %d\n", quit);
    }
    // test:
    printf("Boom Gate Quit before broadcast %p...\n", boom_gate);
    return NULL;
}

// TODO: Remove testing stub
void *generate_cars(void *arg) {

    // for (size_t i = 0; i < NUM_OF_CARS; i++) {
    //     printf("Generating Car %li ...\n", i);
    //     sleep(2);
    // }
    // return NULL;


    
    int bool_for_checking = 0;
    char file_location[] = "plates.txt";
    printf("%s\n", file_location );
    FILE *f = fopen(file_location, "r");
    if (!f)
    {
        perror ("File not exising");
        return (NULL);
        bool_for_checking = 1; 
    }
   

    //create two d array  //int 100 is 100 plates in .txt
    char ** plates = calloc (100, sizeof(char *));
    // int plates_len; 
    //each elements in 2d array allocate memory into it
    for(size_t i = 0; i < 100; i++ )
    {
        char * each_plateSize = calloc(6, sizeof(char));
        plates[i] = each_plateSize;
    }
    
    int line = 0; 
    char buffer[100];

    while((line< 100) && (fscanf(f, "%s", buffer) != EOF))
    {
        printf("Read plate %s\n", buffer);
    
        strcpy(plates[line], buffer);
        line++;
    }
    // plates_len = line;
    if (bool_for_checking != 0)
    {
        printf("Couldn't import valid plates!\n");
        return(NULL);
    }
    bool_for_checking = 0; 

    return(NULL);


}

int main() {
    shared_memory_t shm;
    if(sem_unlink(SHM_EST_SEM_NAME)){
        perror("sem_unlink(SHM_EST_SEM_NAME) failed");
    }
    if(sem_unlink(SIM_END_SEM_NAME) != 0){
        perror("shm_unlink(SIM_END_SEM_NAME) failed");
    }
    sem_t *shm_established_sem = sem_open(SHM_EST_SEM_NAME, O_CREAT, 0644, 0);
    sem_t *simulation_ready_sem = sem_open(SIM_READY_SEM_NAME, O_CREAT, 0644, 0);
    sem_t *simulation_ended_sem = sem_open(SIM_END_SEM_NAME, O_CREAT, 0644, 0);
    sem_t *manager_ended_sem = sem_open(MAN_END_SEM_NAME, 0);

    pthread_mutex_init(&initialisation_mutex, NULL);
    pthread_cond_init(&initialisation_cond, NULL);
    printf("Attempt to create shared memory.\n");
    
    if (create_shared_object(&shm)) {
        printf("Created shared memory.\n");
        printf("Init shared data.\n");
        init_shared_memory_data(&shm);
        printf("Finished init shared data.\n");
    } else {
        printf("Shared memory creation failed.\n");
        return -1;
    }

    printf("Waiting for Manager.\n");
    sem_wait(shm_established_sem);

    printf("Manager connected.\n");
    printf("=================.\n");

    pthread_t car_generation_thread;
    pthread_t entrance_threads[ENTRANCES];
    pthread_t exit_threads[EXITS];
    pthread_t level_threads[LEVELS];
    
    printf("Init Entrance threads.\n");

    entrance_t *entrance = malloc(sizeof(entrance_t));
    
    for (int i = 0; i < ENTRANCES; i++) {
        get_entrance(&shm, i, &entrance);
        boom_gate_t* boom_gate = malloc(sizeof(boom_gate_t));
        boom_gate = &entrance->boom_gate;
        pthread_create(&entrance_threads[i], NULL, handle_boom_gate, (void *)boom_gate);
    }
    // exit_t *exit;
    for (int i = 0; i < EXITS; i++) {
        int* data = malloc(sizeof(int));
        *data = i;
        pthread_create(&exit_threads[i], NULL, test_thread, (void *)data);
    }
    // level_t *level;
    for (int i = 0; i < LEVELS; i++) {
        int* data = malloc(sizeof(int));
        *data = i;
        pthread_create(&level_threads[i], NULL, test_thread, (void *)data);
    }
    
    pthread_mutex_lock(&initialisation_mutex);
    int num = 0;
    // TODO: Add number of thread def?
    while(ready_threads < (ENTRANCES + EXITS + LEVELS)){
        if(pthread_cond_wait(&initialisation_cond, &initialisation_mutex) != 0){
            perror("pthread_cond_wait initialisatoin_cond");
            // exit(1);
        };
        num ++;
        printf("Checking condition for %d times.\n", num);
        printf("Ready %d threads.\n", ready_threads);
    }
    pthread_mutex_unlock(&initialisation_mutex);
    // sleep(5);
    sem_post(simulation_ready_sem);
    printf("Finish Init Entrance threads.\n");

    // for (size_t i = 0; i < ENTRANCES; i++) {
    //     get_entrance(&shm, i, &entrance);
    //     boom_gate_t* boom_gate = malloc(sizeof(boom_gate_t));
    //     boom_gate = &entrance->boom_gate;
    //     control_boom_gate(boom_gate, BG_RAISING);
        // sleep(10);
    // }
    
    printf("=================.\n");
    printf("Start Car Thread.\n");
    pthread_create(&car_generation_thread, NULL, generate_cars, NULL); //sub in entry quenes memory 
                                                                        // onto args later

    pthread_join(car_generation_thread, NULL);
    printf("=================.\n");
    printf("Joined Car Thread.\n");
    // sleep(3);
    sem_post(simulation_ended_sem);
    quit = true;
    printf("I'm here.\n");
    // for (int i = 0; i < ENTRANCES; i++) {
    //     get_entrance(&shm, i, &entrance);
    //     // pthread_mutex_lock(&entrance->boom_gate.mutex);
    //     printf("Set Boom Gate %p to Quit...\n", &entrance->boom_gate);
    //     entrance->boom_gate.status = BG_QUIT;
    //     pthread_cond_signal(&entrance->boom_gate.cond);
    //     pthread_join(entrance_threads[i], NULL);
    // }
    // printf("=================.\n");
    // printf("Joined Entrance Threads.\n");

    // for (int i = 0; i < EXITS; i++) {
    //     pthread_join(exit_threads[i], NULL);
    // }
    // printf("=================.\n");
    // printf("Joined Exit Threads.\n");

    // for (int i = 0; i < LEVELS; i++) {
    //     pthread_join(level_threads[i], NULL);
    // }
    // printf("=================.\n");
    // printf("Joined Level Thread.\n");
    // printf("=================.\n");
    // printf("Joined All Threads.\n");

    // sem_post(simulation_ended_sem);
    printf("Wait for manager to end.\n");
    sem_wait(manager_ended_sem);
    clean_shared_memory_data(&shm);
    destroy_shared_object(&shm);

    printf("=================.\n");
    printf("Simulator finished.\n");
    return 0;
}