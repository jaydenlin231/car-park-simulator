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

#include "header.h"

#define SHM_NAME "/PARKING"
#define SHM_SZ sizeof(data_t)

bool get_shared_data(shared_memory_t *shm, const char *share_name) {
    // Get a file descriptor connected to shared memory object and save in
    // shm->fd. If the operation fails, ensure that shm->data is
    // NULL and return false.
    if ((shm->fd = shm_open(share_name, O_RDWR, 0)) < 0) {
        perror("shm_open");
        shm->data = NULL;
        return false;
    }
    // Otherwise, attempt to map the shared memory via mmap, and save the address
    // in shm->data. If mapping fails, return false.
    // INSERT SOLUTION HERE
    if ((shm->data = mmap(0, SHM_SZ, PROT_READ | PROT_WRITE, MAP_SHARED, shm->fd, 0)) == (void *)-1) {
        perror("mmap");
        return false;
    }

    // Modify the remaining stub only if necessary.
    return true;
}

void destroy_shared_object(shared_memory_t *shm) {
    // Remove the shared memory object.
    munmap(shm->data, sizeof(data_t));
    shm->fd = -1;
    shm->data = NULL;
    shm_unlink(shm->name);
}

int main() {
    shared_memory_t shm;
    printf("Manager started.\n");
    if (get_shared_data(&shm, SHM_NAME)) {
        printf("Manager has read it and put in test value\n");
        (shm.data)->test = 1;
    } else {
        printf("Shared memory connection failed.\n");
    }
    destroy_shared_object(&shm);
    printf("Manager finished.\n");

    // printf("%ld\n", sizeof(data_t));

    return 0;
}
