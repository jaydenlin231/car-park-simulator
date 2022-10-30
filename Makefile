all: simulator manager firealarm

simulator: simulator.c shared_memory.c shared_memory.h queue.c queue.h utility.c utility.h simulator_routines.c simulator_routines.h carpark_details.h hashtable.c hashtable.h billing.h billing.c
	gcc simulator.c shared_memory.c queue.c utility.c simulator_routines.c hashtable.c billing.c -o simulator -lpthread -lrt -g -lm -Werror

manager: manager.c shared_memory.c shared_memory.h queue.c queue.h utility.c utility.h manager_routines.c manager_routines.h carpark_details.h capacity.c capacity.h billing.h billing.c
	gcc manager.c shared_memory.c queue.c hashtable.c manager_routines.c utility.c capacity.c billing.c -o manager -lpthread -lrt -lm -g -Werror

firealarm: firealarm.c shared_memory.c shared_memory.h queue.c queue.h utility.c utility.h simulator_routines.c simulator_routines.h carpark_details.h hashtable.c hashtable.h billing.h billing.c
	gcc firealarm.c shared_memory.c queue.c hashtable.c manager_routines.c utility.c capacity.c billing.c -o firealarm -lpthread -lrt -lm -g -Werror

.PHONY: all clean
all: $(EXE)