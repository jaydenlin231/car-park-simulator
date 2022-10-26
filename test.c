#include <errno.h>
#include <fcntl.h>
#include <math.h>
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

#include "hashtable.h"

#define PLATE_FILE "plates.txt"
#define MAX_LINE_LENGTH 7
#define MAX_IMPORTED_PLATES 100

/* msleep(): Sleep for the requested number of milliseconds. */
int msleep(long msec) {
    struct timespec ts;
    int res;

    if (msec < 0) {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}

htab_t import_htable(char fname[]) {
    htab_t htable;
    FILE* textfile;
    char line[MAX_LINE_LENGTH];
    int plates_quantity = 0;

    if (!htab_init(&htable, MAX_IMPORTED_PLATES)) {
        printf("Error initialising htable\n");
    }

    textfile = fopen(fname, "r");
    if (textfile == NULL) {
        printf("Error reading plates file");
    }

    while (plates_quantity < MAX_IMPORTED_PLATES && fscanf(textfile, "%s", line) != EOF) {
        // printf("Read line %s.\n", line);
        char* plate_copy = malloc(sizeof(char) * (strlen(line) + 1));
        strcpy(plate_copy, line);
        plates_quantity++;
        if (htab_find(&htable, plate_copy) != NULL) {
            printf("Duplicated item in htable\n");
            continue;
        }
        if (!htab_add(&htable, plate_copy)) {
            printf("Failed to add item into htable\n");
        }
    }
    fclose(textfile);
    return htable;
}

long double get_time() {
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    long double time = start.tv_sec + (start.tv_nsec / 1e9);
    return time;
}

void start_time(htab_t* h, char* s) {
    item_t* item = htab_find(h, s);
    item->entry_time = get_time();
    item_print(item);
}

void calc_bill(htab_t* h, char* s) {
    item_t* item = htab_find(h, s);
    item->exit_time = get_time();
    long double delta_t = item->exit_time - item->entry_time;
    double rounded = floorf(delta_t * 10000) / 10000;
    printf("%lf\n", rounded);
    item->cost = (rounded / 0.001) * 0.05;
    item_print(item);
}

int main() {
    char* plate = "080UPF";
    int directed_level = 1;
    htab_t hashtable = import_htable(PLATE_FILE);
    // htab_print(&hashtable);
    printf("%s has came through boomgate and is directed to go to %d.\n\n", plate, directed_level);
    start_time(&hashtable, plate);
    msleep(5);
    calc_bill(&hashtable, plate);

    return 0;
}