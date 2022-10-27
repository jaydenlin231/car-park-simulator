#pragma once
#include <stdbool.h>
#include <stdlib.h>

#define MAX_LINE_LENGTH 7
#define MAX_IMPORTED_PLATES 100
// An item inserted into a hash table.
// As hash collisions can occur, multiple items can exist in one bucket.
// Therefore, each bucket is a linked list of items that hashes to that bucket.
typedef struct item item_t;
struct item
{
    char *key;
    int directed_lvl;
    int actual_lvl;
    long double entry_time;
    item_t *next;
};

// A hash table mapping a string to an integer.
typedef struct htab
{
    item_t **buckets;
    size_t size;

} htab_t;

void item_print(item_t *i);
bool htab_init(htab_t *h, size_t n);
size_t djb_hash(char *s);
size_t htab_index(htab_t *h, char *key);
item_t *htab_bucket(htab_t *h, char *key);
item_t *htab_find(htab_t *h, char *key);
bool htab_add(htab_t *h, char *key);
void htab_print(htab_t *h);
void htab_delete(htab_t *h, char *key);
void htab_destroy(htab_t *h);
htab_t import_htable(char fname[]);
