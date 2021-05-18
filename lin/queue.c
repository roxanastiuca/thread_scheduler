#include "queue.h"

/*
 * Description: initialize new queue.
 * Return: NULL if memory allocation fails, else queue.
 */
queue_t *new_queue(TPriorityFunc priority_func)
{
	queue_t *q = (queue_t *) malloc(sizeof(queue_t));

	if (q) {
		q->front = q->back = NULL;
		q->priority = priority_func;
	}

	return q;
}

/*
 * Description: frees entire memory used by queue, using
 * free_elem for freeing space held by elements.
 */
void free_queue(queue_t **q, TFreeEl free_elem)
{
	free_list(&((*q)->front), free_elem);
	free(*q);
	*q = NULL;
}

/*
 * Description: inserts one element in queue at the correct place
 * referred by its priority.
 * Return: -ENOMEM, else 0 for no error.
 */
int push_back(queue_t *q, void *val)
{
	list_t elem = new_node(val);
	
	if (!elem)
		return -ENOMEM;

	if (q->front == NULL) {
		/* first element added */
		q->front = q->back = elem;
		return 0;
	}

	if (q->priority(q->back->val) >= q->priority(val)) {
		/* element is placed in the back */
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
		/* place between ant and l */
		elem->next = l;
		ant->next = elem;
	}

	return 0;
}

/*
 * Description: removes and returns first element in queue or
 * NULL if queue is empty.
 */
void *pop_front(queue_t *q)
{
	if (!q->front)
		return NULL;

	list_t elem = q->front;
	void *val = elem->val;
	
	q->front = elem->next;
	free(elem);

	return val;
}

/*
 * Description: returns first element in queue or NULL if queue is
 * empty.
 */
void *peek_front(queue_t *q)
{
	if (!q->front)
		return NULL;

	return q->front->val;
}
