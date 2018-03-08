#ifndef __LINKED_LIST_H
#define __LINKED_LIST_H

#define OUT_OF_MEMORY_ERROR -1
#define ELEM_NOT_FOUND -2
#define SUCCESS 1

typedef int (*equal_Fn)(void * elem1, void * elem2);

typedef struct llist_elem_struct{

	void * item;
	struct llist_elem_struct * next;
	struct llist_elem_struct * prev;

}
llist_elem;

typedef struct llist_struct{

	int size;
	llist_elem * head;
	llist_elem * tail;
	equal_Fn equals;
}
llist;

typedef void (*transformFn)(void * p, void * args);

void llist_init(llist * l);
int llist_append(llist *l, void * item);
void * llist_get(llist * l, int index);
int llist_get_pos(llist *l, void * item);
void * llist_remove_at(llist * l, int index);
void llist_requeue(llist * l);
void * llist_pop(llist * l);
void * llist_remove_at(llist * l, int index);
int llist_remove(llist * l, void *item);
int llist_size(llist * l);
void llist_iterate(llist * l, void (*act_on)(void *item, void * args), void * args);
void llist_destroy(llist * l);
void llist_set_equality_checker(llist * l, int (*equality_checker)(void * elem1, void * elem2));
int llist_remove_free(llist * l, void * item);

#endif
