#ifndef _STULIB_H
#define _STULIB_H

#include <stddef.h>

#define containerof(ptr, type, member)  ((type *)((char *)(ptr) - offsetof(type, member)))

void xxd(unsigned char *buf, unsigned int len, void (*print)(const unsigned char *buffer));

typedef _Bool bool;
enum { false = 0, true = 1 };

//
// Doubly linked list
//

struct list_node {
	struct list_node *next;
	struct list_node *prev;
};

static inline void init_list_head(struct list_node *node) {
	node->next = node;
	node->prev = node;
}

static inline bool list_is_empty(struct list_node *node) {
	return node->next == node;
}

static inline void list_add(struct list_node *new,
                            struct list_node *node) {
	new->next = node->next;
	new->prev = node;
	new->next->prev = new;
	new->prev->next = new;
}

static inline void list_del(struct list_node *node) {
	node->next->prev = node->prev;
	node->prev->next = node->next;
}

//
// Red Black Tree
//

struct rb_node {
	struct rb_node *left;
	struct rb_node *right;
};

static inline void rb_insert(struct rb_node *node, bool (cmp)(struct rb_node *a, struct rb_node *b)) {
	// TODO
	(void)node;
	(void)cmp(NULL, NULL);
}

#endif
