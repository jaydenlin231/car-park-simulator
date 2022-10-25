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
#include "queue.h"
#include "hashtable.h"

#define SHM_NAME "/PARKING"
#define SHM_SZ sizeof(data_t)

#define SHM_EST_SEM_NAME "/SHM_ESTABLISHED"
#define SIM_READY_SEM_NAME "/SIM_READY"
#define SIM_END_SEM_NAME "/SIM_ENDED"
#define MAN_READY_SEM_NAME "/MAN_READY"
#define MAN_END_SEM_NAME "/MAN_ENDED"


// TODO: Remove testing stub
#define NUM_OF_CARS 5

pthread_mutex_t initialisation_mutex;
pthread_cond_t initialisation_cond;

sem_t *manager_ended_sem;

// TODO: Remove testing stub
int ready_threads = 0;

// typedef struct sh_entrance_data {
//     entrance_t *entrances[ENTRANCES];
//     queue_t *entrance_queues[ENTRANCES];
//     pthread_mutex_t mutexs[ENTRANCES];
//     pthread_cond_t conds[ENTRANCES];
// } sh_entrance_data_t;

typedef struct entrance_data {
    entrance_t *entrance;
    queue_t *entrance_queue;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} entrance_data_t;

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

void *wait_manager_close(void *data)
{
    printf("Manager monitor thread waiting.\n");
    sem_wait(manager_ended_sem);
    printf("Manager monitor thread Woke\n");
    return NULL;
}

void *control_boom_gate(boom_gate_t *boom_gate, char update_status) {
    if(update_status == BG_RAISING) {
            pthread_mutex_lock(&boom_gate->mutex);
            printf("Instruct Boom Gate %p Raising\n", boom_gate);
            boom_gate->status = BG_RAISING;
            pthread_mutex_unlock(&boom_gate->mutex);
            pthread_cond_broadcast(&boom_gate->cond);

        } else if(update_status == BG_LOWERING) {
            printf("Instruct Boom Gate Lowering\n");
            pthread_mutex_lock(&boom_gate->mutex);
            boom_gate->status = BG_LOWERING;
            pthread_mutex_unlock(&boom_gate->mutex);
            pthread_cond_broadcast(&boom_gate->cond);
        }
    return NULL;
}

void *handle_entrance_queue(){

    pthread_mutex_lock(&initialisation_mutex);
    ready_threads++;
    pthread_cond_broadcast(&initialisation_cond);
    pthread_mutex_unlock(&initialisation_mutex);

    // while (true) {
    //     pthread_mutex_lock();
        
    //     while (!(boom_gate->status == BG_RAISING || boom_gate->status == BG_LOWERING)) {
    //         pthread_cond_wait(&boom_gate->cond, &boom_gate->mutex);
    //     }
    //     pthread_mutex_unlock();
    // }

}

void *handle_boom_gate(void *data) {
    boom_gate_t *boom_gate = (boom_gate_t *)data;

    printf("Created Boom Gate %p Thread\n", boom_gate);

    pthread_mutex_lock(&initialisation_mutex);
    ready_threads++;
    pthread_cond_broadcast(&initialisation_cond);
    pthread_mutex_unlock(&initialisation_mutex);
    // int num = 0;
    int test = 0;
    while (true) {
        printf("\tBoom Gate %p Monitor Loop ran %d times.\n",boom_gate, ++test);

        if(pthread_mutex_lock(&boom_gate->mutex)!=0){
            exit(1);
        };
        
        while (!(boom_gate->status == BG_RAISING || boom_gate->status == BG_LOWERING)) {
            printf("\tBoom Gate %p Cond Wait BG_RAISING or BG_LOWERING. Current Status: %c.\n", boom_gate, boom_gate->status);
             if(pthread_cond_wait(&boom_gate->cond, &boom_gate->mutex)!=0){
                exit(1);
             };
        }
         if(pthread_mutex_unlock(&boom_gate->mutex)!=0){
            exit(1);
        };
        
        printf("Boom Gate %p Received Instruction Status: %c.\n", boom_gate, boom_gate->status);
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
    }
    printf("Boom Gate Quit before broadcast %p...\n", boom_gate);
    return NULL;
}

void *generate_cars(void *arg) {
    entrance_data_t *entrance_datas = (entrance_data_t *) arg;
    // queue_t *entrance_queue = entrance_data[]->entrance_queue;

    pthread_mutex_t lock_rand_num = PTHREAD_MUTEX_INITIALIZER;
    int bool_for_checking = 0;
    char file_location[] = "plates.txt";
    // printf("%s\n", file_location );
    FILE *f = fopen(file_location, "r");
    if (!f)
    {
        perror ("File not exising");
        return (NULL);
        bool_for_checking = 1; 
    }
    int total_plates;
   

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
        return(NULL);
    }
    bool_for_checking = 0; 

    while(true)
    {
        sleep(1);
        // char result;
        pthread_mutex_lock(&lock_rand_num);
        int halfChance = rand() % 2;
        pthread_mutex_unlock(&lock_rand_num);

        char six_d_plate[7];
        if(halfChance == 1)
        {
            pthread_mutex_lock(&lock_rand_num);

            int index;
            index = (rand() % ((total_plates - 1) + 0 - 1)) +0;
            pthread_mutex_unlock(&lock_rand_num);
            for (size_t i = 0; i < 3; i++)
            {
                six_d_plate[i] = plates[index][i];
            }

            for (size_t i = 3; i < 6; i++)
            {
                six_d_plate[i] = plates[index][i];
            }
            for (size_t i = 6; i<7; i++ )
            {
                six_d_plate[i] = '\0';
            }
        }
        else
        {
            pthread_mutex_lock(&lock_rand_num);
            for (int i=0; i<3;i++){
                six_d_plate[i] = '0' + rand() % 9;
                six_d_plate[i+3] = 'A' + rand() % 26;
            }
            for (size_t i = 6; i<7; i++ )
            {
                six_d_plate[i] = '\0';
            }
            pthread_mutex_unlock(&lock_rand_num);
        }
        int rand_entrance = rand() % 5;
        printf("%s queued to %p: queue #%d \n", six_d_plate, &entrance_datas[rand_entrance].entrance_queue, rand_entrance);

        char *plate_string = malloc(sizeof(char) * 7);
        // plate_string[6] = '\0';
        strcpy(plate_string, six_d_plate);
        if(pthread_mutex_lock(&entrance_datas[rand_entrance].mutex)!=0){
            perror("pthread_mutex_lock failed");
        };
        enqueue(entrance_datas[rand_entrance].entrance_queue, plate_string);
        if(pthread_mutex_unlock(&entrance_datas[rand_entrance].mutex)!=0){
            perror("pthread_mutex_unlock failed");
        };

        for(int i = 0; i < ENTRANCES; i++){
            print_queue(entrance_datas[i].entrance_queue);
        }
        printf("\n");

    }
    

    return(NULL);

   
}


int main() {
    shared_memory_t shm;
    if(sem_unlink(SHM_EST_SEM_NAME) !=0){
        perror("sem_unlink(SHM_EST_SEM_NAME) failed");
    }
    if(sem_unlink(SIM_READY_SEM_NAME) != 0){
        perror("shm_unlink(SIM_READY_SEM_NAME) failed");
    }
    if(sem_unlink(SIM_END_SEM_NAME) != 0){
        perror("shm_unlink(SIM_END_SEM_NAME) failed");
    }
    if(sem_unlink(MAN_READY_SEM_NAME) != 0){
        perror("shm_unlink(SIM_END_SEM_NAME) failed");
    }
    if(sem_unlink(MAN_END_SEM_NAME) != 0){
        perror("shm_unlink(SIM_END_SEM_NAME) failed");
    }
    
    
    sem_t *shm_established_sem = sem_open(SHM_EST_SEM_NAME, O_CREAT, 0644, 0);
    sem_t *simulation_ready_sem = sem_open(SIM_READY_SEM_NAME, O_CREAT, 0644, 0);
    sem_t *simulation_ended_sem = sem_open(SIM_END_SEM_NAME, O_CREAT, 0644, 0);
    sem_t *manager_ready_sem = sem_open(MAN_READY_SEM_NAME, O_CREAT, 0644, 0);
    manager_ended_sem = sem_open(MAN_END_SEM_NAME, O_CREAT, 0644, 0);

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

    queue_t* entrance_queues[ENTRANCES];
    // sh_entrance_data_t *sh_entrance_data = malloc(sizeof(sh_entrance_data_t));

    entrance_data_t entrance_datas[ENTRANCES];

    printf("Init Entrance threads.\n");

    entrance_t *entrance = malloc(sizeof(entrance_t));
    for (int i = 0; i < ENTRANCES; i++) {
        get_entrance(&shm, i, &entrance);
        // sh_entrance_data->entrance_queues[i] = create_queue();
        // sh_entrance_data->entrances[i] = entrance;
        // pthread_mutex_init(&sh_entrance_data->mutexs[i], NULL);
        // pthread_cond_init(&sh_entrance_data->conds[i], NULL);

        entrance_datas[i].entrance = entrance;
        entrance_datas[i].entrance_queue = create_queue();
        pthread_mutex_init(&entrance_datas[i].mutex, NULL);
        pthread_cond_init(&entrance_datas[i].cond, NULL);
        
        boom_gate_t* boom_gate = malloc(sizeof(boom_gate_t));
        boom_gate = &entrance->boom_gate;
        pthread_create(&entrance_threads[i], NULL, handle_boom_gate, (void *)boom_gate);
        pthread_create(&entrance_queue_threads[i], NULL, handle_entrance_queue, NULL);
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
    pthread_create(&monitor_sim_thread, NULL, wait_manager_close, NULL);
    
    printf("Checking Init Entrance threads.\n");
    pthread_mutex_lock(&initialisation_mutex);
    int num = 0;
    // TODO: Add number of thread def?
    while(ready_threads < (ENTRANCES)){
        pthread_cond_wait(&initialisation_cond, &initialisation_mutex);
        num ++;
        printf("Checking condition for %d times.\n", num);
        printf("Ready %d threads.\n", ready_threads);
    }
    pthread_mutex_unlock(&initialisation_mutex);
    printf("Finish Init Entrance threads.\n");
    
    sem_post(simulation_ready_sem);
    printf("Simulation ready to start\n");
    printf("Waiting for manager ready\n");

    sem_wait(manager_ready_sem);

    for (int i = 0; i < ENTRANCES; i++) {
        if(i % 2 == 1){
            printf("Instructing Entrance %d\n", i);
            get_entrance(&shm, i, &entrance);
            boom_gate_t* boom_gate = malloc(sizeof(boom_gate_t));
            boom_gate = &entrance->boom_gate;
            control_boom_gate(boom_gate, BG_RAISING);
            sleep(2);
        }
    }
    
    printf("=================.\n");
    printf("Start Car Thread.\n");
    pthread_create(&car_generation_thread, NULL, generate_cars, (void*) &entrance_datas);

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