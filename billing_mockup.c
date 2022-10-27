#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

/* msleep(): msleep for the requested number of milliseconds. */
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

int main()
{
    long double start_time = get_time();
    printf("%Lf\n", start_time);
    msleep(5);
    long double end_time = get_time();
    printf("%Lf\n", end_time);

    long double delta_t = end_time - start_time;
    printf("delta t: %Lf\n", delta_t);
    return 0;
}