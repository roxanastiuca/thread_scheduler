#include "queue.h"

queue_t *new_queue(TPriorityFunc priority_func) {
	queue_t *q = (queue_t *) malloc(sizeof(queue_t));

	if (q) {
		q->front = q->back = NULL;
		q->priority = priority_func;
	}

	return q;
}

void free_queue(queue_t **q, TFreeEl free_elem) {
	free_list(&((*q)->front), free_elem);
	free(*q);
	*q = NULL;
}

int push_back(queue_t *q, void *val) {
	list_t elem = new_node(val);
	if (!elem)
		return -ENOMEM;

	if (q->front == NULL) {
		/* first element added */
		q->front = q->back = elem;
		return 0;
	}

	if (q->priority(q->back->val) >= q->priority(val)) {
		/* place element in the back */
		q->back->next = elem;
		q->back = elem;
		return 0;
	}

	/* parse through list q->front until priority(elem) > */
	list_t l = q->front, ant = NULL;

	while (l && q->priority(val) <= q->priority(l->val)) {
		ant = l;
		l = l->next;
	}

	if (ant == NULL) {
		/* place first in list */
		elem->next = q->front;
		q->front = elem;
	} else {
		elem->next = l;
		ant->next = elem;
	}

	return 0;
}

void *pop_front(queue_t *q) {
	if (!q->front)
		return NULL;

	list_t elem = q->front;
	void *val = elem->val;
	q->front = elem->next;
	free(elem);

	return val;
}

void *peek_front(queue_t *q) {
	if (!q->front)
		return NULL;

	return q->front->val;
}