#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "queue.h"

// A utility function to create a new linked list node.
queue_node_t *new_node(char *plate)
{
	queue_node_t *temp = (queue_node_t *)malloc(sizeof(queue_node_t));
	temp->plate = plate;
	temp->next = NULL;
	return temp;
}

// A utility function to create an empty queue
queue_t *create_queue()
{
	queue_t *q = (queue_t *)malloc(sizeof(queue_t));
	q->front = q->rear = NULL;
	return q;
}

// The function to add a plate char pointer to q
void enqueue(queue_t *q, char *plate)
{
	// Create a new LL node
	queue_node_t *temp = new_node(plate);

	// If queue is empty, then new node is front and rear
	// both
	if (q->rear == NULL)
	{
		q->front = q->rear = temp;
		return;
	}

	// Add the new node at the end of queue and change rear
	q->rear->next = temp;
	q->rear = temp;
}

// Function to remove the first plate char pointer from given queue q and return the char pointer
char *dequeue(queue_t *q)
{
	// If queue is empty, return NULL.
	if (q->front == NULL)
		return NULL;

	// Store previous front and move front one node ahead
	queue_node_t *temp = q->front;

	q->front = q->front->next;

	// If front becomes NULL, then change rear also as NULL
	if (q->front == NULL)
		q->rear = NULL;
	char *ret = temp->plate;
	free(temp);
	return ret;
}

bool is_empty(queue_t *q)
{
	if (q->front == NULL)
	{
		return true;
	}
	return false;
}

void print_queue(queue_t *q)
{
	if (is_empty(q))
	{
		printf("The queue is empty\n");
		return;
	}

	queue_node_t *temp = q->front;

	while (temp != NULL)
	{
		printf("%s ", temp->plate);
		temp = temp->next;
	}
	printf("\n");
}