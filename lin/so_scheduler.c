#include "so_scheduler.h"
#include "queue.h"
#include "list.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <semaphore.h>

#include <stdio.h>

/* possible thread states: */
typedef enum {
	NEW,
	READY,
	RUNNING,
	WAITING,
	TERMINATED
} state_t;

/* information about one thread in the scheduler: */
typedef struct {
	pthread_t tid; /* thread ID */
	so_handler *handler; /* handler function */	
	int priority; /* thread priority */
	state_t state; /* current state */
	unsigned int time_remaining; /* used while running */

	/*
	 * synchronization elements for marking when thread has
	 * been planned and for when it is running. */
	sem_t is_planned;
	sem_t is_running;
} thread_t;

/* information about scheduler state */
typedef struct {
	unsigned int time_quantum; /* quantum for running thread */
	unsigned int io_devices; /* number of IO devices supported */
	queue_t *ready; /* priority queue for READY threads */
	queue_t *finished; /* queue for TERMINATED threads */
	queue_t **waiting_io; /* array of queues in WAITING state */
	thread_t *running; /* currently running thread */

	int threads_no; /* number of threads forked */
	sem_t stop; /* mark when it's alright to proceed with so_end */
} scheduler_t;

/* Function for READY priority queue. */
int check_thread_priority(void *addr)
{
	return ((thread_t *) addr)->priority;
}

/* 
 * Function used to use a queue_t* as normal queue (all elements have
 * priority 0. */
int check_terminated_thread_priority(void *addr)
{
	return 0;
}

scheduler_t *scheduler;

void *thread_func(void *args);
void plan_new_thread(thread_t *t);
void thread_finished();

/*
 * Description: initialize scheduler and all its components.
 * Return: 0 for no error/ negative value for error.
 */
int so_init(unsigned int time_quantum, unsigned int io)
{
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
		scheduler->waiting_io[i] =
			new_queue(check_terminated_thread_priority);
		if (!scheduler->waiting_io[i])
			return -ENOMEM;
	}

	scheduler->threads_no = 0;
	sem_init(&scheduler->stop, 0, 0);

	return 0;
}

/*
 * Description: start new thread and plan its execution.
 * Return: thread ID.
 */
tid_t so_fork(so_handler *func, unsigned int priority)
{
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
	(scheduler->threads_no)++;

	rc = pthread_create(&t->tid, NULL, thread_func, t);
	if (rc != 0) {
		free(t);
		return INVALID_TID;
	}


	/* Wait until thread has been consumed. */
	sem_wait(&t->is_planned);

	/* Consume one instruction: */
	if (scheduler->running != t)
		so_exec();

	return t->tid;
}

/*
 * Description: block thread until signaled.
 * Return: 0 for no error, else negative number.
 */
int so_wait(unsigned int io)
{

	if (io >= scheduler->io_devices)
		return -1;

	thread_t *t = scheduler->running;
	t->state = WAITING;
	push_back(scheduler->waiting_io[io], t);

	/* Pass execution to next READY thread. */
	scheduler->running = pop_front(scheduler->ready);
	if (scheduler->running != NULL)
		sem_post(&scheduler->running->is_running);

	/* Wait until back to RUNNING state. */
	sem_wait(&t->is_running);

	return 0;
}

/*
 * Description: unblock all threads waiting for IO device.
 * Return: 0 for no error, else negative number.
 */
int so_signal(unsigned int io)
{
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

	/* Consume one instruction: */
	so_exec();
	return count;
}

/*
 * Description: simulate the execution of one instruction. Decrement
 * quantum for RUNNING thread and do preemption if necessary.
 */
void so_exec(void)
{
	if (scheduler->running == NULL)
		return;

	thread_t *t = scheduler->running;

	t->time_remaining--;
	if (t->time_remaining == 0) {
		/* Move RUNNING thread to READY queue. */
		t->state = READY;
		t->time_remaining = scheduler->time_quantum;
		push_back(scheduler->ready, t);

		/* Get next READY thread that isn't TERMINATED. */
		thread_t *next = pop_front(scheduler->ready);

		while (next && next->state == TERMINATED) {
			push_back(scheduler->finished, next);
			next = pop_front(scheduler->ready);
		}

		scheduler->running = next;
		if (scheduler->running) {
			/* Notify thread that it's running. */
			scheduler->running->state = RUNNING;
			sem_post(&scheduler->running->is_running);
		}
	} else if (peek_front(scheduler->ready) &&
		t->priority < ((thread_t *)peek_front(scheduler->ready))->priority) {
		/* 
		 * Exchange current RUNNING thread with READY thread with higher
		 * priority. */
		t->state = READY;
		push_back(scheduler->ready, t);
		scheduler->running = pop_front(scheduler->ready);
		scheduler->running->state = RUNNING;
		sem_post(&scheduler->running->is_running);
	}

	/* If preemption happened, wait until back to running. */
	if (t != scheduler->running)
		sem_wait(&t->is_running);	
}

/*
 * Description: join thread and free all memory for it.
 */
void free_thread(void *addr)
{
	thread_t *t = (thread_t *) addr;

	pthread_join(t->tid, NULL);
	sem_destroy(&t->is_planned);
	sem_destroy(&t->is_running);
	free(t);
}

/*
 * Description: wait for all threads to terminate and free scheduler
 * resources.
 */
void so_end(void)
{
	if (scheduler) {
		/* Wait until threads terminate. */
		if (scheduler->threads_no != 0)
			sem_wait(&scheduler->stop);

		free_queue(&scheduler->finished, free_thread);
		free_queue(&scheduler->ready, free_thread);

		if (scheduler->running)
			free_thread(&scheduler->running);

		for (int i = 0; i < scheduler->io_devices; i++)
			free_queue(&scheduler->waiting_io[i], free);

		free(scheduler->waiting_io);
		free(scheduler);
		sem_destroy(&scheduler->stop);
	}

	scheduler = NULL;
}

/*
 * Description: plan new thread. Either set it on RUNNING or READY.
 */
void plan_new_thread(thread_t *t)
{
	if (scheduler->running == NULL) {
		/* only thread on system; set it to run */
		t->state = RUNNING;
		scheduler->running = t;
		sem_post(&scheduler->running->is_running);
		return;
	}

	if (t->priority > scheduler->running->priority) {
		/* preemption */
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

/* Thread has finished */
void thread_finished(void)
{
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

/*
 * Description: thread routine.
 */
void *thread_func(void *args)
{
	thread_t *t = (thread_t *) args;

	/* plan this thread and notify it's been planned */
	plan_new_thread(t);
	sem_post(&t->is_planned);

	/* wait until this thread is running */
	sem_wait(&t->is_running);

	t->handler(t->priority);
	
	t->state = TERMINATED;
	thread_finished();

	return NULL;
}
