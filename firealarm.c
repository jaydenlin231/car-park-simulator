#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

int shm_fd;
volatile void *shm;

int alarm_active = 0;
pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t alarm_condvar = PTHREAD_COND_INITIALIZER;

#define LEVELS 5
#define ENTRANCES 5
#define EXITS 5

#define MEDIAN_WINDOW 5
#define TEMPCHANGE_WINDOW 30

struct boomgate {
	pthread_mutex_t m;
	pthread_cond_t c;
	char s;
};
struct parkingsign {
	pthread_mutex_t m;
	pthread_cond_t c;
	char display;
};

struct tempnode {
	int temperature;
	struct tempnode *next;
};

struct tempnode *deletenodes(struct tempnode *templist, int after)
{
	if (templist->next) {
		templist->next = deletenodes(templist->next, after - 1);
	}
	if (after <= 0) {
		free(templist);
		return NULL;
	}
	return templist;
}
int compare(const void *first, const void *second)
{
	return *((const int *)first) - *((const int *)second);
}

void tempmonitor(int level)
{
	struct tempnode *templist = NULL, *newtemp, *medianlist = NULL, *oldesttemp;
	int count, addr, temp, mediantemp, hightemps;
	
	for (;;) {
		// Calculate address of temperature sensor
		addr = 0150 * level + 2496;
		temp = *((int16_t *)(shm + addr));
		
		// Add temperature to beginning of linked list
		newtemp = malloc(sizeof(struct tempnode));
		newtemp->temperature = temp;
		newtemp->next = templist;
		templist = newtemp;
		
		// Delete nodes after 5th
		deletenodes(templist, MEDIAN_WINDOW);
		
		// Count nodes
		count = 0;
		for (struct tempnode *t = templist; t != NULL; t = t->next) {
			count++;
		}
		
		if (count == MEDIAN_WINDOW) { // Temperatures are only counted once we have 5 samples
			int *sorttemp = malloc(sizeof(int) * MEDIAN_WINDOW);
			count = 0;
			for (struct tempnode *t = templist; t != NULL; t = t->next) {
				sorttemp[count++] = t->temperature;
			}
			qsort(sorttemp, MEDIAN_WINDOW, sizeof(int), compare);
			mediantemp = sorttemp[(MEDIAN_WINDOW - 1) / 2];
			
			// Add median temp to linked list
			newtemp = malloc(sizeof(struct tempnode));
			newtemp->temperature = mediantemp;
			newtemp->next = medianlist;
			medianlist = newtemp;
			
			// Delete nodes after 30th
			deletenodes(medianlist, TEMPCHANGE_WINDOW);
			
			// Count nodes
			count = 0;
			hightemps = 0;
			
			for (struct tempnode *t = medianlist; t != NULL; t = t->next) {
				// Temperatures of 58 degrees and higher are a concern
				if (t->temperature >= 58) hightemps++;
				// Store the oldest temperature for rate-of-rise detection
				oldesttemp = t;
				count++;
			}
			
			if (count == TEMPCHANGE_WINDOW) {
				// If 90% of the last 30 temperatures are >= 58 degrees,
				// this is considered a high temperature. Raise the alarm
				if (hightemps >= TEMPCHANGE_WINDOW * 0.9)
					alarm_active = 1;
				
				// If the newest temp is >= 8 degrees higher than the oldest
				// temp (out of the last 30), this is a high rate-of-rise.
				// Raise the alarm
				if (templist->temperature - oldesttemp->temperature >= 8)
					alarm_active = 1;
			}
		}
		
		usleep(2000);
		
	}
}

void *openboomgate(void *arg)
{
	struct boomgate *bg = arg;
	pthread_mutex_lock(&bg->m);
	for (;;) {
		if (bg->s == 'C') {
			bg->s = 'R';
			pthread_cond_broadcast(&bg->c);
		}
		if (bg->s == 'O') {
		}
		pthread_cond_wait(&bg->c, &bg->m);
	}
	pthread_mutex_unlock(&bg->m);
	
}

int main()
{
	shm_fd = shm_open("PARKING", O_RDWR, 0);
	shm = (volatile void *) mmap(0, 2920, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	
	pthread_t *threads = malloc(sizeof(pthread_t) * LEVELS);
	
	for (int i = 0; i < LEVELS; i++) {
		pthread_create(threads + i, NULL, (void *(*)(void *)) tempmonitor, (void *)i);
	}
	for (;;) {
		if (alarm_active) {
			goto emergency_mode;
		}
		usleep(1000);
	}
	
	emergency_mode:
	fprintf(stderr, "*** ALARM ACTIVE ***\n");
	
	// Handle the alarm system and open boom gates
	// Activate alarms on all levels
	for (int i = 0; i < LEVELS; i++) {
		int addr = 0150 * i + 2498;
		char *alarm_trigger = (char *)shm + addr;
		*alarm_trigger = 1;
	}
	
	// Open up all boom gates
	pthread_t *boomgatethreads = malloc(sizeof(pthread_t) * (ENTRANCES + EXITS));
	for (int i = 0; i < ENTRANCES; i++) {
		int addr = 288 * i + 96;
		volatile struct boomgate *bg = shm + addr;
		pthread_create(boomgatethreads + i, NULL, openboomgate, bg);
	}
	for (int i = 0; i < EXITS; i++) {
		int addr = 192 * i + 1536;
		volatile struct boomgate *bg = shm + addr;
		pthread_create(boomgatethreads + ENTRANCES + i, NULL, openboomgate, bg);
	}
	
	// Show evacuation message on an endless loop
	for (;;) {
		char *evacmessage = "EVACUATE ";
		for (char *p = evacmessage; *p != '\0'; p++) {
			for (int i = 0; i < ENTRANCES; i++) {
				int addr = 288 * i + 192;
				volatile struct parkingsign *sign = shm + addr;
				pthread_mutex_lock(&sign->m);
				sign->display = *p;
				pthread_cond_broadcast(&sign->c);
				pthread_mutex_unlock(&sign->m);
			}
			usleep(20000);
		}
	}
	
	for (int i = 0; i < LEVELS; i++) {
		pthread_join(threads[i], NULL);
	}
	
	munmap((void *)shm, 2920);
	close(shm_fd);
}
