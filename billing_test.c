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

// gcc -o billing_test billing_test.c hashtable.c -lm

/* msleep(): Sleep for the requested number of milliseconds. */
int msleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do
    {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}

long double get_time()
{
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    long double time = start.tv_sec + (start.tv_nsec / 1e9);
    return time;
}

void start_time(htab_t *h, char *s)
{
    item_t *item = htab_find(h, s);
    if (item != NULL)
    {
        item->entry_time = get_time();
        item_print(item);
    }
    else
    {
        printf("Plate not found in hastable\n");
    }
}

void calc_bill(htab_t *h, char *s)
{
    item_t *item = htab_find(h, s);
    if (item != NULL)
    {
        long double exit_time = get_time();
        long double delta_time = exit_time - item->entry_time;
        int delta_ms = delta_time * 1000;
        int delta_ms_rounded = ((delta_ms + 5 / 2) / 5) * 5;
        double rounded = delta_ms_rounded / 1000.0;
        printf("parked for %lf seconds\n", rounded);
        double bill = rounded * 50;

        // Write to billing.txt file
        FILE *fp;
        fp = fopen("billing.txt", "a");
        if (!fprintf(fp, "%s $%.2lf\n", item->key, bill))
        {
            printf("Error writing\n");
        }
        fclose(fp);
        item->entry_time = 0;
    }
    else
    {
        printf("Plate not found in hastable\n");
    }
}

int main()
{
    char *plate = "168BUT";
    int directed_level = 1;
    htab_t hashtable = import_htable(PLATE_FILE);
    printf("%s has came through boomgate and is directed to go to %d.\n", plate, directed_level);
    // As the car goes through entrance start_time
    start_time(&hashtable, plate);

    // Random time the car parks inside the parking
    // msleep(249);
    usleep(249000);

    // As the car leaves exit call calc_bill
    calc_bill(&hashtable, plate);

    // htab_print(&hashtable);
    return 0;
}