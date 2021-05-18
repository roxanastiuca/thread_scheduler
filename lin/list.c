#include "list.h"

list_t new_node(void *val) {
	list_t node = (list_t) malloc(sizeof(node_t));

	if (node) {
		node->val = val;
		node->next = NULL;
	}

	return node;
}

void free_list(list_t *addr_list, TFreeEl free_elem) {
	list_t l = *addr_list;
	*addr_list = NULL;

	while (l) {
		list_t curr = l;
		l = l->next;

		free_elem(curr->val);
		free(curr);
	}
}

int insert(list_t *addr_list, void *val) {
	list_t node = new_node(val);
	if (!node)
		return -ENOMEM;

	node->next = *addr_list;
	*addr_list = node;

	return 0;
}