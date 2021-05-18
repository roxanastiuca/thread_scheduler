/* definition of generic singly-linked lists */

#ifndef LIST_H
#define LIST_H

#include <stdlib.h>
#include <errno.h>

typedef void (*TFreeEl)(void *);

typedef struct node {
	void *val; /* address where value is stored */
	struct node *next; /* pointer to next node */
} node_t, *list_t;

list_t new_node(void *val);
void free_list(list_t *addr_list, TFreeEl free_elem);

#endif

