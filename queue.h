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
