#include "list.h"

/*
 * Description: allocate memory for new list node.
 * Return: NULL if memory allocation fails, else the node.
 */
list_t new_node(void *val)
{
	list_t node = (list_t) malloc(sizeof(node_t));

	if (node) {
		node->val = val;
		node->next = NULL;
	}

	return node;
}

/*
 * Description: free space for entire list, calling free_elem
 * to free space for each element.
 */
void free_list(list_t *addr_list, TFreeEl free_elem)
{
	list_t l = *addr_list;

	while (l) {
		list_t curr = l;
		
		l = l->next;
		free_elem(curr->val);
		free(curr);
	}

	*addr_list = NULL;
}

