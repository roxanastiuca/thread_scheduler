all: build

libscheduler.so: build

build: so_scheduler.o list.o queue.o
	gcc -shared so_scheduler.o list.o queue.o -o libscheduler.so -Wall -g -ggdb

so_scheduler.o: so_scheduler.c
	gcc -Wall -fPIC -g so_scheduler.c -c -o so_scheduler.o -ggdb

list.o: list.c
	gcc -Wall -fPIC -g list.c -c -o list.o -ggdb

queue.o: queue.c
	gcc -Wall -fPIC -g queue.c -c -o queue.o -ggdb

clean:
	rm *.o libscheduler.so
