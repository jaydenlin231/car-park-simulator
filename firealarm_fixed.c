#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define SEG_NAME "/PARKING"
#define SEG_SIZE 2920
#define QUEUE_SIZE 10000

#define TIME_SCALE 1
// Structre of carpark
#define NUM_ENTRANCES 5
#define NUM_EXITS 5
#define NUM_LEVEL 5
#define VEHICLES_PER_LVL 20

// Firealarm defines
#define MEDIAN_WINDOW 5
#define TEMPCHANGE_WINDOW 30
int lv_id[NUM_LEVEL];

#define FILE_LOC "plates.txt"

int16_t shm_fd;
void *shm;

int16_t alarm_active = 0;
pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t alarm_condvar = PTHREAD_COND_INITIALIZER;

struct boomgate
{
	pthread_mutex_t m;
	pthread_cond_t c;
	char s;
};
struct parkingsign
{
	pthread_mutex_t m;
	pthread_cond_t c;
	char display;
};

struct tempnode
{
	int temperature;
	struct tempnode *next;
};

struct tempnode *deletenodes(struct tempnode *templist, int after)
{
	if (templist->next)
	{
		templist->next = deletenodes(templist->next, after - 1);
	}
	if (after <= 0)
	{
		free(templist);
		return NULL;
	}
	return templist;
}

int compare(const void *first, const void *second)
{
	return *((const int *)first) - *((const int *)second);
}

void *tempmonitor(void *levelArg)
{
	int level = (*(int *)levelArg);
	struct tempnode *templist = malloc(sizeof(struct tempnode));
	struct tempnode *newtemp;
	struct tempnode *medianlist = malloc(sizeof(struct tempnode));
	struct tempnode *oldesttemp;
	int count;
	int addr;
	int temp;
	int mediantemp;
	int hightemps;
	char *sign;

	// intptr_t level = (intptr_t) levelArg;
	while (alarm_active == 0)
	{ // !!NASA Power of 10: #2 (loops have fixed bounds)!
		// Calculate address of temperature sensor
		addr = 104 * level + 2496;
		char firstChar = *((char *)(shm + addr)) - '0';
		char secondChar = *((char *)(shm + addr + 1)) - '0';
		temp = firstChar * 10 + secondChar;

		// Add temperature to beginning of linked lis
		newtemp = malloc(sizeof(struct tempnode));
		newtemp->temperature = temp;
		newtemp->next = templist;
		templist = newtemp;

		// Delete nodes after 5th
		deletenodes(templist, MEDIAN_WINDOW);

		// Count nodes
		count = 0;
		for (struct tempnode *t = templist; t != NULL; t = t->next)
		{
			count++;
		}

		if (count == MEDIAN_WINDOW)
		{ // Temperatures are only counted once we have 5 samples
			int *sorttemp = malloc(sizeof(int) * MEDIAN_WINDOW);
			count = 0;
			for (struct tempnode *t = templist; t != NULL; t = t->next)
			{
				count++;
				sorttemp[count] = t->temperature;
			}
			qsort(sorttemp, MEDIAN_WINDOW, sizeof(int), compare);
			mediantemp = sorttemp[(MEDIAN_WINDOW - 1) / 2];

			// Add median temp to linked list
			newtemp = malloc(sizeof(struct tempnode));
			newtemp->temperature = mediantemp;
			// printf("Median Temp:%d\n", mediantemp);
			newtemp->next = medianlist;
			medianlist = newtemp;

			// Delete nodes after 30th
			deletenodes(medianlist, TEMPCHANGE_WINDOW);

			// Count nodes
			count = 0;
			hightemps = 0;

			for (struct tempnode *t = medianlist; t != NULL; t = t->next)
			{
				// Temperatures of 58 degrees and higher are a concern
				if (t->temperature >= 58)
					hightemps++;
				// Store the oldest temperature for rate-of-rise detection
				oldesttemp = t;
				count++;
				if (count > 30)
				{
					break;
				}
			}

			if (count == TEMPCHANGE_WINDOW)
			{
				// If 90% of the last 30 temperatures are >= 58 degrees,
				// this is considered a high temperature. Raise the alarm
				if (hightemps >= TEMPCHANGE_WINDOW * 0.9)
				{
					// printf("Option 1\n");
					alarm_active = 1;
					sign = shm + addr + 2;
					*sign = 1;
				}

				// If the newest temp is >= 8 degrees higher than the oldest
				// temp (out of the last 30), this is a high rate-of-rise.
				// Raise the alarm
				if (templist->temperature - oldesttemp->temperature >= 8 && oldesttemp->temperature != 0)
				{

					// printf("Option 1\n");
					alarm_active = 1;
					sign = shm + addr + 2;
					*sign = 1;
				}
			}
		}

		usleep(2000);
	}
	return NULL;
}

void *open_en_boomgate(void *arg)
{
	// struct boomgate *bg = arg;
	int i = *((int *)arg);
	struct boomgate *bg = shm + 288 * i + 96;

	do
	{
		pthread_mutex_lock(&bg->m);
		if (bg->s == 'C')
		{
			bg->s = 'R';
			pthread_mutex_unlock(&bg->m);
			pthread_cond_broadcast(&bg->c);
		}
		else if (bg->s == 'O')
		{
			pthread_cond_wait(&bg->c, &bg->m);
		}
		else
		{
			pthread_mutex_unlock(&bg->m);
		}
	} while (bg->s != 'O');
	return NULL;
}

void *open_ex_boomgate(void *arg)
{
	// struct boomgate *bg = arg;
	int i = *((int *)arg);
	struct boomgate *bg = shm + 192 * i + 1536;

	do
	{
		pthread_mutex_lock(&bg->m);
		if (bg->s != 'O')
		{
			bg->s = 'R';
			pthread_mutex_unlock(&bg->m);
			pthread_cond_broadcast(&bg->c);
		}
		else if (bg->s == 'O')
		{
			pthread_cond_wait(&bg->c, &bg->m);
		}
		else
		{
			pthread_mutex_unlock(&bg->m);
		}
	} while (bg->s != 'O');
	return NULL;
}

int main()
{
	int en_id[NUM_ENTRANCES];
	int ex_id[NUM_EXITS];
	shm_fd = shm_open(SEG_NAME, O_RDWR, 0666);
	ftruncate(shm_fd, SEG_SIZE);
	shm = (void *)mmap(0, SEG_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

	pthread_t *threads = malloc(sizeof(pthread_t) * NUM_LEVEL);
	for (intptr_t i = 0; i < NUM_LEVEL; i++)
	{
		lv_id[i] = i;

		pthread_create(threads + i, NULL, tempmonitor, (void *)&lv_id[i]);
	}
	while (!alarm_active)
	{
		usleep(1000);
	}
	fprintf(stderr, "*** ALARM ACTIVE ***\n");
	if (alarm_active)
	{
		// Handle the alarm system and open boom gates
		// Activate alarms on all levels
		for (int i = 0; i < NUM_LEVEL; i++)
		{
			int addr = 0150 * i + 2498; // no octat
			char *alarm_trigger = (char *)shm + addr;
			*alarm_trigger = '1';
		}
		// Open up all boom gates
		pthread_t *boomgatethreads = malloc(sizeof(pthread_t) * (NUM_ENTRANCES + NUM_EXITS));
		for (int i = 0; i < NUM_ENTRANCES; i++)
		{
			en_id[i] = i;

			// int addr = 288 * i + 96;
			// volatile struct boomgate *bg = shm + addr;
			// pthread_cond_broadcast((void *)&bg->c);
			// pthread_create(boomgatethreads + i, NULL, openboomgate, (void*)bg);
			pthread_create(boomgatethreads + i, NULL, open_en_boomgate, (void *)&en_id[i]);
		}
		for (int i = 0; i < NUM_EXITS; i++)
		{
			ex_id[i] = i;

			// int addr = 192 * i + 1536;
			// volatile struct boomgate *bg = shm + addr;
			// pthread_cond_broadcast((void *)&bg->c);
			// pthread_create(boomgatethreads + NUM_ENTRANCES + i, NULL, openboomgate, (void*)bg);
			pthread_create(boomgatethreads + NUM_ENTRANCES + i, NULL, open_ex_boomgate, (void *)&ex_id[i]);
		}

		// Show evacuation message on an endless loop
		while (alarm_active == 1)
		{
			if ((*(char *)(shm + 2919)) == 1)
			{
				break;
			}
			char *evacmessage = "EVACUATE ";
			for (char *p = evacmessage; *p != '\0'; p++)
			{
				for (int i = 0; i < NUM_ENTRANCES; i++)
				{
					int addr = 288 * i + 192;
					struct parkingsign *sign = shm + addr;
					pthread_mutex_lock((void *)&sign->m);
					sign->display = *p;
					pthread_cond_broadcast((void *)&sign->c);
					pthread_mutex_unlock((void *)&sign->m);
				}
				usleep(20000 * TIME_SCALE);
			}
		}
	}

	// for (int i = 0; i < NUM_LEVEL; i++) {
	// 	pthread_join(threads[i], NULL);
	// }

	munmap((void *)shm, 2920);
	close(shm_fd);
	return 0;
}