
#include "linkedlist.h"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>

int equals(void * elem1, void * elem2) {
	if (elem1 == elem2)
		return 0;
	else
		return 1;
}

void llist_init(llist * l) {
	l->head = NULL;
	l->tail = NULL;
	l->size = 0;
	l->equals = equals;
}

int llist_append(llist *l, void * item) {

	llist_elem * new_elem;
	new_elem = (llist_elem *) kmalloc(sizeof(llist_elem), GFP_KERNEL);
	if (!new_elem) {
		printk(KERN_INFO "LLIST Append: NOMEM\n");
		BUG();
	}

	if (item == NULL)
		printk(KERN_INFO "Appending Null Item\n");

	new_elem->item = item;
	l->size ++;

	if (l->head == NULL) {

		l->head = new_elem;
		l->tail = new_elem;
		l->head->next = NULL;
		l->head->prev = NULL;
		l->tail->next = NULL;
		l->tail->prev = NULL;
		return SUCCESS;
	}

	l->tail->next = new_elem;
	new_elem->next = NULL;
	new_elem->prev = l->tail;
	l->tail = new_elem;
	return SUCCESS;
}

void * llist_get(llist * l, int index) {

	int i = 0;
	llist_elem * head = l->head;
	while (head != NULL) {
		if (i == index) {
			if (head->item == NULL)
				printk(KERN_INFO "Item is NULL\n");
			return head->item;
		}
		i++;
		head = head->next;
	}

	printk(KERN_INFO "LLIST Head is NULL\n");
	return NULL;

}

int llist_get_pos(llist *l, void * item) {
	int i = 0;
	llist_elem * head = l->head;
	while (head != NULL) {
		if (l->equals(head->item, item) == 0) {
			return i;
		}
		i++;
		head = head->next;
	}
	return ELEM_NOT_FOUND;
}

void llist_set_equality_checker(llist * l,
                                int (*equality_checker)(void * elem1,
                                        void * elem2)) {
	l->equals = equality_checker;
}

void llist_requeue(llist * l) {

	if (!l)
		return;

	if (l->size == 0 || l->size == 1)
		return;

	llist_elem * head = l->head;
	llist_elem * tail = l->tail;

	l->head = l->head->next;
	l->head->prev = NULL;

	l->tail->next = head;
	head->prev = l->tail;
	l->tail = head;
	l->tail->next = NULL;
	return;
}


void * llist_remove_at(llist * l, int index) {

	if (!l)
		return NULL;

	int i = 0;
	llist_elem * head = l->head;
	void * result;
	while (head != NULL) {
		if (i == index) {
			result =  head->item;
			l->size --;
			if (head == l->head) {
				if (l->size == 0) {
					l->head = NULL;
					l->tail = NULL;
					kfree(head);
					return result;
				}

				if (head->next != NULL)
					head->next->prev = NULL;

				l->head = head->next;
				kfree(head);
				return result;
			}

			if (head == l->tail) {

				l->tail = l->tail->prev;
				if (l->tail != NULL)
					l->tail->next = NULL;
				else {
					l->head = NULL;
					l->tail = NULL;
					l->size = 0;
				}

				kfree(head);
				return result;
			}

			if (head->prev != NULL)
				head->prev->next = head->next;
			else {
				printk(KERN_INFO "LLIST Warning Should not happen\n");
			}

			if (head->next != NULL) {
				head->next->prev = head->prev;
			} else {
				printk(KERN_INFO "LLIST Warning Should not happen\n");
			}

			kfree(head);
			return result;
		}
		i++;
		head = head->next;
	}
	return NULL;

}

int llist_size(llist * l) {
	return l->size;
}

void * llist_pop(llist * l) {
	if (l != NULL) {
		if (l->size == 0)
			return NULL;

		llist_elem * head = l->head;
		llist_elem * tail = l->tail;
		void * result = head->item;

		if (l->size == 1) {
			l->head = NULL;
			l->tail = NULL;
			kfree(head);
		} else {
			l->head = l->head->next;
			l->head->prev = NULL;
			kfree(head);
		}

		l->size = l->size - 1;
		return result;

	} else
		return NULL;
}

int llist_remove(llist *l, void * item) {
	int i = 0;
	void * elem;
	llist_elem * head = l->head;
	while (head != NULL) {
		elem = head->item;
		if (l->equals(elem, item) == 0) {
			llist_remove_at(l, i);
			return SUCCESS;
		}
		head = head->next;
		i++;
	}
	return ELEM_NOT_FOUND;

}

int llist_remove_free(llist *l, void * item) {
	int i = 0;
	void * elem;
	llist_elem * head = l->head;

	while (head != NULL) {
		elem = head->item;
		if (l->equals(elem, item) == 0) {
			llist_remove_at(l, i);
			kfree(elem);
			return SUCCESS;
		}
		head = head->next;
		i++;
	}
	return ELEM_NOT_FOUND;

}


void llist_iterate(llist * l, void (*act_on)(void *item, void * args),
                   void * args) {

	int i;
	void * elem;
	for (i = 0; i < l->size; i++) {
		elem = llist_get(l, i);
		if (elem != NULL)
			act_on(elem, args);
	}
}

void llist_destroy(llist * l) {
	while (l->size > 0)
		llist_pop(l);
}
