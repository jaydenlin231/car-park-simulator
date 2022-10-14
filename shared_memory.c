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

bool create_shared_object(shared_memory_t *shm) {
    // Remove any previous instance of the shared memory object, if it exists.
    if(shm_unlink(SHM_NAME) != 0){
        perror("shm_unlink() failed");
    }

    // Assign share name to shm->name.
    shm->name = SHM_NAME;

    // Create the shared memory object, allowing read-write access, and saving the
    // resulting file descriptor in shm->fd. 
    if ((shm->fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666)) < 0) {
        perror("shm_open error");
        shm->data = NULL;
        return false;
    }

    // Set the capacity of the shared memory object via ftruncate. If the 
    // operation fails.
    if (ftruncate(shm->fd, SHM_SZ)) {
        perror("ftruncate error");
        shm->data = NULL;
        return false;
    }
    // Attempt to map the shared memory via mmap, and save the address
    // in shm->data. 
    if ((shm->data = mmap(0, SHM_SZ, PROT_WRITE, MAP_SHARED, shm->fd, 0)) == (char *)-1) {
        perror("mmap error");
        return false;
    }

    return true;
}

void destroy_shared_object(shared_memory_t *shm) {
    // Remove the shared memory object.
    if(munmap(shm->data, SHM_SZ)!= 0){
        perror("munmap() failed");
    }
    if(shm_unlink(shm->name) != 0){
        perror("shm_unlink() failed");
    }
    shm->fd = -1;
    shm->data = NULL;
}

bool get_shared_object(shared_memory_t *shm) {
    // Get a file descriptor connected to shared memory object and save in
    // shm->fd. 
    if ((shm->fd = shm_open(SHM_NAME, O_RDWR, 0)) < 0) {
        perror("shm_open");
        shm->data = NULL;
        return false;
    }
    // Otherwise, attempt to map the shared memory via mmap, and save the address
    // in shm->data.
    if ((shm->data = mmap(0, SHM_SZ, PROT_READ | PROT_WRITE, MAP_SHARED, shm->fd, 0)) == (void *)-1) {
        perror("mmap");
        return false;
    }

    return true;
}