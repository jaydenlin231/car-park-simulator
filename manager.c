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
    printf("Manager started.\n");
    if (get_shared_object(&shm)) {
        printf("Manager has read it and put in test value\n");
        (shm.data)->test = 1;
    } else {
        printf("Shared memory connection failed.\n");
    }
    printf("Manager finished.\n");

    // printf("%ld\n", sizeof(data_t));

    return 0;
}
