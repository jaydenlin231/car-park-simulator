// A linked list (LL) node to store a queue entry
typedef struct queue_node_t queue_node_t;

// A linked list (LL) node to store a queue entry
struct queue_node_t {
	char *plate;
	queue_node_t *next;
};

// The queue, front stores the front node of LL and rear
// stores the last node of LL
typedef struct queue {
	queue_node_t *front;
	queue_node_t *rear;
}queue_t;

queue_node_t *new_node(char *plate);
queue_t  *create_queue();
void enqueue(queue_t  * q, char *plate);
char *dequeue(queue_t  * q);
bool is_empty(queue_t *q);
void print_queue(queue_t *q);
