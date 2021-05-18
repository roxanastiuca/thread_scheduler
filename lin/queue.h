#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include "list.h"
#include <errno.h>

typedef int (*TPriorityFunc)(void *);

typedef struct {
	list_t front, back;
	TPriorityFunc priority;
} queue_t;

queue_t *new_queue(TPriorityFunc priority_func);
void free_queue(queue_t **q, TFreeEl free_elem);
int push_back(queue_t *q, void *val);
void *pop_front(queue_t *q);
void *peek_front(queue_t *q);

#endif