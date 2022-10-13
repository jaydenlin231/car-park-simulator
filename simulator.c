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

#define LEVELS 5
#define ENTRANCES 5
#define EXITS 5
#define SHM_NAME "/PARKING"
#define SHM_SZ 2920

bool create_shm(shared_memory_t *shm) {
    shm_unlink(SHM_NAME);

    shm->name = SHM_NAME;

    if ((shm->fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666)) < 0) {
        shm->data = NULL;
        perror("shm_open error");
        return false;
    }

    if (ftruncate(shm->fd, SHM_SZ)) {
        shm->data = NULL;
        perror("ftruncate error");
        return false;
    }
    if ((shm->data = mmap(0, SHM_SZ, PROT_WRITE, MAP_SHARED, shm->fd, 0)) == (char *)-1) {
        perror("mmap error");
        return false;
    }
    return true;
}

int main() {
}