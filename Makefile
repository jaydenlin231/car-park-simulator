all: simulator manager

simulator: simulator.c shared_memory.c shared_memory.h queue.c queue.h utility.c utility.h simulator_routines.c simulator_routines.h carpark_details.h
	gcc simulator.c shared_memory.c queue.c utility.c simulator_routines.c -o simulator -lpthread -lrt

manager: manager.c shared_memory.c shared_memory.h queue.c queue.h utility.c utility.h manager_routines.c manager_routines.h carpark_details.h capacity.c capacity.h
	gcc manager.c shared_memory.c queue.c hashtable.c manager_routines.c utility.c capacity.c -o manager -lpthread -lrt