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

#include "carpark_types.h"
#include "shared_memory.h"

#define SHM_NAME "/PARKING"
#define SHM_SZ sizeof(data_t)

int main() {
    shared_memory_t shm;
    // create_shm(&shm);
    printf("Simulator started.\n");
    if (create_shared_object(&shm)) {
        printf("Address from Simulator when create_shm.\n");
        while (shm.data->test == 0) {
            usleep(5000);
        }
    } else {
        printf("Shared memory creation failed.\n");
    }

    if (get_shared_object(&shm)) {
        printf("Address from Simulator when get_shared_data.\n");
    } else {
        printf("Shared memory connection failed.\n");
    }
    destroy_shared_object(&shm);
    printf("Simulator finished.\n");
}