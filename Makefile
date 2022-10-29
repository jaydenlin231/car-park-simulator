all: simulator manager firealarm_fixed firealarm_new_shm

simulator: simulator.c shared_memory.c shared_memory.h queue.c queue.h utility.c utility.h simulator_routines.c simulator_routines.h carpark_details.h hashtable.c hashtable.h billing.h billing.c
	gcc simulator.c shared_memory.c queue.c utility.c simulator_routines.c hashtable.c billing.c -o simulator -lpthread -lrt -g -lm

manager: manager.c shared_memory.c shared_memory.h queue.c queue.h utility.c utility.h manager_routines.c manager_routines.h carpark_details.h capacity.c capacity.h billing.h billing.c
	gcc manager.c shared_memory.c queue.c hashtable.c manager_routines.c utility.c capacity.c billing.c -o manager -lpthread -lrt -lm -g

firealarm_fixed: firealarm_fixed.c shared_memory.c shared_memory.h queue.c queue.h utility.c utility.h simulator_routines.c simulator_routines.h carpark_details.h hashtable.c hashtable.h billing.h billing.c
	gcc firealarm_fixed.c shared_memory.c queue.c hashtable.c manager_routines.c utility.c capacity.c billing.c -o firealarm_fixed -lpthread -lrt -lm -g

firealarm_new_shm: firealarm_new_shm.c shared_memory.c shared_memory.h queue.c queue.h utility.c utility.h simulator_routines.c simulator_routines.h carpark_details.h hashtable.c hashtable.h billing.h billing.c
	gcc firealarm_new_shm.c shared_memory.c queue.c hashtable.c manager_routines.c utility.c capacity.c billing.c -o firealarm_new_shm -lpthread -lrt -lm -g