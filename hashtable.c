#include "hashtable.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

void item_print(item_t *i)
{
    printf("%s- directed level: %d, actual level: %d\n", i->key, i->directed_lvl, i->actual_lvl);
}

// Initialise a new hash table with n buckets.
// pre: true
// post: (return == false AND allocation of table failed)
//       OR (all buckets are null pointers)
bool htab_init(htab_t *h, size_t n)
{
    pthread_mutex_init(&h->mutex, NULL);
    h->size = n;
    h->buckets = (item_t **)calloc(n, sizeof(item_t *));
    return h->buckets != 0;
}

// The Bernstein hash function.
// A very fast hash function that works well in practice.
size_t djb_hash(char *s)
{
    size_t hash = 5381;
    int c;
    while ((c = *s++) != '\0')
    {
        // hash = hash * 33 + c
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

// Calculate the offset for the bucket for key in hash table.
size_t htab_index(htab_t *h, char *key)
{
    return djb_hash(key) % h->size;
}

// Find pointer to head of list for key in hash table.
item_t *htab_bucket(htab_t *h, char *key)
{
    return h->buckets[htab_index(h, key)];
}

// Find an item for key in hash table.
// pre: true
// post: (return == NULL AND item not found)
//       OR (strcmp(return->key, key) == 0)
item_t *htab_find(htab_t *h, char *key)
{
    for (item_t *i = htab_bucket(h, key); i != NULL; i = i->next)
    {
        if (strcmp(i->key, key) == 0)
        { // found the key
            return i;
        }
    }
    return NULL;
}
// Add a key with value to the hash table.
// pre: htab_find(h, key) == NULL
// post: (return == false AND allocation of new item failed)
//       OR (htab_find(h, key) != NULL)
bool htab_add(htab_t *h, char *key)
{
    // allocate new item
    item_t *newhead = (item_t *)malloc(sizeof(item_t));
    if (newhead == NULL)
    {
        return false;
    }

    newhead->key = key;

    size_t bucket = htab_index(h, key);
    newhead->next = h->buckets[bucket];
    h->buckets[bucket] = newhead;

    return true;
}

// Print the hash table.
// pre: true
// post: hash table is printed to screen
void htab_print(htab_t *h)
{
    printf("hash table with %zu buckets\n", h->size);
    for (size_t i = 0; i < h->size; ++i)
    {
        printf("bucket %zu: ", i);
        if (h->buckets[i] == NULL)
        {
            printf("empty\n");
        }
        else
        {
            for (item_t *j = h->buckets[i]; j != NULL; j = j->next)
            {
                item_print(j);
                if (j->next != NULL)
                {
                    printf(" -> ");
                }
            }
            printf("\n");
        }
    }
}

// Delete an item with key from the hash table.
// pre: htab_find(h, key) != NULL
// post: htab_find(h, key) == NULL
void htab_delete(htab_t *h, char *key)
{
    item_t *head = htab_bucket(h, key);
    item_t *current = head;
    item_t *previous = NULL;
    while (current != NULL)
    {
        if (strcmp(current->key, key) == 0)
        {
            if (previous == NULL)
            { // first item in list
                h->buckets[htab_index(h, key)] = current->next;
            }
            else
            {
                previous->next = current->next;
            }
            free(current);
            break;
        }
        previous = current;
        current = current->next;
    }
}

// Destroy an initialised hash table.
// pre: htab_init(h)
// post: all memory for hash table is released
void htab_destroy(htab_t *h)
{
    // free linked lists
    for (size_t i = 0; i < h->size; ++i)
    {
        item_t *bucket = h->buckets[i];
        while (bucket != NULL)
        {
            item_t *next = bucket->next;
            free(bucket);
            bucket = next;
        }
    }

    // free buckets array
    free(h->buckets);
    h->buckets = NULL;
    h->size = 0;
}

// Import lines of car rego to hastable
htab_t import_htable(char fname[])
{
    htab_t htable;
    FILE *textfile;
    char line[MAX_LINE_LENGTH];
    int plates_quantity = 0;

    if (!htab_init(&htable, MAX_IMPORTED_PLATES))
    {
        printf("Error initialising htable\n");
    }

    textfile = fopen(fname, "r");
    if (textfile == NULL)
    {
        printf("Error reading plates file");
    }

    while (plates_quantity < MAX_IMPORTED_PLATES && fscanf(textfile, "%s", line) != EOF)
    {
        // printf("Read line %s.\n", line);
        char *plate_copy = malloc(sizeof(char) * (strlen(line) + 1));
        strcpy(plate_copy, line);
        plates_quantity++;
        if (htab_find(&htable, plate_copy) != NULL)
        {
            printf("Duplicated item in htable\n");
            continue;
        }
        if (!htab_add(&htable, plate_copy))
        {
            printf("Failed to add item into htable\n");
        }
    }
    fclose(textfile);
    return htable;
}
