# car-park-simulator

Helpful vid: https://www.youtube.com/watch?v=oHg5SJYRHA0

Compile commands 
Mac
gcc-12 -o simulator simulator.c shared_memory.c queue.c -lpthread
gcc-12 -o manager manager.c shared_memory.c queue.c hashtable.c -lpthread

WSL
gcc -o simulator simulator.c shared_memory.c queue.c -lpthread -lrt
gcc -o manager manager.c shared_memory.c queue.c hashtable.c -lpthread -lrt
