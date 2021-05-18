#include "so_scheduler.h"
#include "queue.h"
#include "list.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <semaphore.h>

#include <stdio.h>

typedef enum {
	NEW,
	READY,
	RUNNING,
	WAITING,
	TERMINATED
} state_t;

typedef struct {
	pthread_t tid;
	so_handler *handler;
	unsigned int io;
	int priority;
	state_t state;
	unsigned int time_remaining;

	sem_t is_planned;
	sem_t is_running;
} thread_t;

typedef struct {
	unsigned int time_quantum;
	unsigned int io_devices;
	queue_t *ready, *finished, **waiting_io;
	thread_t *running;

	int threads_no;
	sem_t stop;
} scheduler_t;

int check_thread_priority(void *addr) {
	return ((thread_t *) addr)->priority;
}

int check_terminated_thread_priority(void *addr) {
	return 0;
}

scheduler_t *scheduler = NULL;

void *thread_func(void *args);
void plan_new_thread(thread_t *t);
void thread_finished();

int so_init(unsigned int time_quantum, unsigned int io) {
	if (scheduler != NULL ||
		io > SO_MAX_NUM_EVENTS || time_quantum <= 0)
		return -1;

	scheduler = (scheduler_t *) malloc(sizeof(scheduler_t));
	if (!scheduler)
		return -ENOMEM;

	scheduler->time_quantum = time_quantum;
	scheduler->io_devices = io;
	scheduler->running = NULL;
	scheduler->ready = new_queue(check_thread_priority);
	if (!scheduler->ready) {
		free(scheduler);
		return -ENOMEM;
	}
	scheduler->finished = new_queue(check_terminated_thread_priority);
	if (!scheduler->finished) {
		free_queue(&scheduler->ready, free);
		free(scheduler);
		return -ENOMEM;
	}
	scheduler->waiting_io = (queue_t **) calloc(io, sizeof(queue_t *));
	if (!scheduler->waiting_io) {
		free_queue(&scheduler->ready, free);
		free_queue(&scheduler->finished, free);
		free(scheduler);
		return -ENOMEM;
	}
	for (int i = 0; i < io; i++) {
		scheduler->waiting_io[i] = new_queue(check_terminated_thread_priority);
		if (!scheduler->waiting_io[i])
			return -ENOMEM;
	}

	scheduler->threads_no = 0;
	sem_init(&scheduler->stop, 0, 0);

	return 0;
}

tid_t so_fork(so_handler *func, unsigned int priority) {
	int rc;
	thread_t *t;

	if (!func || priority > SO_MAX_PRIO)
		return INVALID_TID;

	t = (thread_t *) malloc(sizeof(thread_t));
	if (!t)
		return INVALID_TID;

	t->priority = priority;
	t->state = NEW;
	t->time_remaining = scheduler->time_quantum;
	t->handler = func;
	sem_init(&t->is_planned, 0, 0);
	sem_init(&t->is_running, 0, 0);
	scheduler->threads_no++;

	rc = pthread_create(&t->tid, NULL, thread_func, t);
	if (rc != 0) {
		free(t);
		return INVALID_TID;
	}


	// asteapta ca threadul sa fie planificat
	sem_wait(&t->is_planned);

	// Consume one instruction:
	if (scheduler->running != t)
		so_exec();

	return t->tid;
}

int so_wait(unsigned int io) {

	if (io >= scheduler->io_devices)
		return -1;

	thread_t *t = scheduler->running;
	t->state = WAITING;
	push_back(scheduler->waiting_io[io], t);

	scheduler->running = pop_front(scheduler->ready);
	if (scheduler->running != NULL)
		sem_post(&scheduler->running->is_running);

	sem_wait(&t->is_running);

	return 0;
}

int so_signal(unsigned int io) {

	if (io >= scheduler->io_devices)
		return -1;

	thread_t *t = pop_front(scheduler->waiting_io[io]);
	int count = 0;

	while (t) {
		t->state = READY;
		push_back(scheduler->ready, t);

		count++;
		t = pop_front(scheduler->waiting_io[io]);
	}

	so_exec();

	return count;
}

void so_exec(void) {
	if (scheduler->running == NULL)
		return;

	thread_t *t = scheduler->running;

	t->time_remaining--;
	if (t->time_remaining == 0) {

		t->state = READY;
		t->time_remaining = scheduler->time_quantum;
		push_back(scheduler->ready, t);

		thread_t *next = pop_front(scheduler->ready);
		while (next && next->state == TERMINATED) {
			push_back(scheduler->finished, next);
			next = pop_front(scheduler->ready);
		}

		scheduler->running = next;
		if (scheduler->running) {
			scheduler->running->state = RUNNING;
			sem_post(&scheduler->running->is_running);
		}
	} else if (peek_front(scheduler->ready) &&
		t->priority < ((thread_t *)peek_front(scheduler->ready))->priority) {

		t->state = READY;
		push_back(scheduler->ready, t);
		scheduler->running = pop_front(scheduler->ready);
		scheduler->running->state = RUNNING;
		sem_post(&scheduler->running->is_running);
	}

	if (t != scheduler->running)
		sem_wait(&t->is_running);	
}

void free_thread(void *addr) {
	thread_t *t = (thread_t *) addr;
	pthread_join(t->tid, NULL);
	sem_destroy(&t->is_planned);
	sem_destroy(&t->is_running);
	free(t);
}

void so_end(void) {
	if (scheduler) {
		if (scheduler->threads_no != 0)
			sem_wait(&scheduler->stop);

		free_queue(&scheduler->ready, free_thread);
		free_queue(&scheduler->finished, free_thread);

		if (scheduler->running) {
			free_thread(&scheduler->running);
		}
		for (int i = 0; i < scheduler->io_devices; i++)
			free_queue(&scheduler->waiting_io[i], free);
		free(scheduler->waiting_io);
		free(scheduler);
		sem_destroy(&scheduler->stop);
	}

	scheduler = NULL;
}

// new thread forked
void plan_new_thread(thread_t *t) {

	if (scheduler->running == NULL) {
		// only thread on system; set it to run
		t->state = RUNNING;
		scheduler->running = t;
		sem_post(&scheduler->running->is_running);
		return;
	}

	if (t->priority > scheduler->running->priority) {
		// preemption
		thread_t *aux = scheduler->running;
		scheduler->running = t;
		t->state = RUNNING;
		aux->state = READY;
		push_back(scheduler->ready, aux);
		return;
	}

	t->state = READY;
	push_back(scheduler->ready, t);
}

// currently running thread has finished
void thread_finished() {
	thread_t *t = scheduler->running;


	if (t->state == TERMINATED) {
		push_back(scheduler->finished, t);
		scheduler->running = pop_front(scheduler->ready);

		if (scheduler->running != NULL)
			sem_post(&scheduler->running->is_running);

		if (scheduler->running == NULL && peek_front(scheduler->ready) == NULL)
			sem_post(&scheduler->stop);

	}

}

void *thread_func(void *args) {
	thread_t *t = (thread_t *) args;

	// plan this thread and notify it's been planned
	plan_new_thread(t);
	sem_post(&t->is_planned);

	// wait until this thread is running
	sem_wait(&t->is_running);

	t->handler(t->priority);
	
	t->state = TERMINATED;
	thread_finished();

	return NULL;
}