#include "billing.h"
#include <math.h>

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