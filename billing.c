#include "billing.h"
#include <math.h>

// Get current time
long double get_time()
{
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    long double time = start.tv_sec + (start.tv_nsec / 1e9);
    return time;
}

// Start car timing
void start_time(htab_t *h, char *s)
{
    item_t *item = htab_find(h, s);
    if (item != NULL)
    {
        item->entry_time = get_time();
    }
}

// Calculate bill of car and write to file
void calc_bill(htab_t *h, char *s, double *total)
{
    item_t *item = htab_find(h, s);
    if (item != NULL)
    {
        long double exit_time = get_time();
        long double delta_time = exit_time - item->entry_time;
        int delta_ms = delta_time * 1000;
        int delta_ms_rounded = ((delta_ms + 5 / 2) / 5) * 5;
        double rounded = delta_ms_rounded / 1000.0;
        double bill = rounded * 50;

        // Write to billing.txt file
        FILE *fp;
        fp = fopen("billing.txt", "a");
        *total += bill;
        if (!fprintf(fp, "%s $%.2lf\n", item->key, bill))
        {
            printf("Error writing\n");
        }
        fclose(fp);
        item->entry_time = 0;
    }
}