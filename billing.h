#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <unistd.h>

#include "hashtable.h"

long double get_time();
void start_time(htab_t *h, char *s);
void calc_bill(htab_t *h, char *s);