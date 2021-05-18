#ifndef LIST_H
#define LIST_H

#include <stdlib.h>
#include <errno.h>

typedef void (*TFreeEl)(void *);

typedef struct node {
	void *val;
	struct node *next;
} node_t, *list_t;

list_t new_node(void *val);
void free_list(list_t *addr_list, TFreeEl free_elem);
int insert(list_t *addr_list, void *val);

#endif