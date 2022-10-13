#include <fcntl.h>
#include <pthread.h>
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

int main() {
    LPR_t A;

    printf("Size of struct: %ld\n", sizeof(A));

    return 0;
}
