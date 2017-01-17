
#include "linkedlist.h"
//#include <stdio.h>
//#include <stdlib.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>

int equals(void * elem1, void * elem2){

	if(elem1 == elem2)
		return 0;
	else
		return 1;

}

void llist_init(llist * l){

	l->head = NULL;
	l->tail = NULL;
	l->size = 0;
	l->equals = equals;
}

int llist_append(llist *l, void * item){

	llist_elem * new_elem;
	
	new_elem = (llist_elem *) kmalloc(sizeof(llist_elem), GFP_KERNEL);
	if(new_elem == NULL)
		return OUT_OF_MEMORY_ERROR;

	new_elem->item = item;	
	l->size ++;

	if(l->head == NULL){

		l->head = new_elem;
		l->tail = new_elem;
		l->head->next = NULL;
		l->head->prev = NULL;
		return SUCCESS;
	}
	
	l->tail->next = new_elem;
	new_elem->next = NULL;
	new_elem->prev = l->tail;
	l->tail = new_elem;
	return SUCCESS;

}

void * llist_get(llist * l, int index){
	
	int i = 0;
	llist_elem * head = l->head;
	while(head != NULL){
		if(i == index){
			return head->item;
		}
		i++;
		head = head->next;
	}
	return NULL;

}

int llist_get_pos(llist *l, void * item){
	int i = 0;
	llist_elem * head = l->head;
	while(head != NULL){
		if(l->equals(head->item,item) == 0){
			return i;
		}
		i++;
		head = head->next;
	}
	return ELEM_NOT_FOUND; 

}

void llist_set_equality_checker(llist * l, int (*equality_checker)(void * elem1, void * elem2)){
	l->equals = equality_checker;
}

void * llist_remove_at(llist * l, int index){

	int i = 0;
	llist_elem * head = l->head;
	void * result;
	while(head != NULL){
		if(i == index){
			result =  head->item;
			l->size --;
			if(head == l->head){
				if(l->head == l->tail){
					l->head = NULL;
					l->tail = NULL;
					kfree(head);
					return result;
				}
				
				head->next->prev = NULL;
				l->head = head->next;
				kfree(head);
				return result;
			}
			
			if(head == l->tail){
				
				l->tail = l->tail->prev;
				if(l->tail != NULL)
				l->tail->next = NULL;
				kfree(head);
				return result;
			}			

			head->prev->next = head->next;
			head->next->prev = head->prev;
			kfree(head);
			return result;
		}
		i++;
		head = head->next;
	}
	return NULL;

}

int llist_size(llist * l){
	return l->size;
}

void * llist_pop(llist * l){

	return llist_remove_at(l,0);
}

int llist_remove(llist *l, void * item){
	int i = 0;
	void * elem;
	llist_elem * head = l->head;
	while(head != NULL){
		elem = head->item;
		if(l->equals(elem,item) == 0){
			llist_remove_at(l,i);
			return SUCCESS;
		}
		head = head->next;
		i++;
	}
	return ELEM_NOT_FOUND;

}

int llist_remove_free(llist *l, void * item){
	int i = 0;
	void * elem;
	llist_elem * head = l->head;

	while(head != NULL){
		elem = head->item;
		if(l->equals(elem,item) == 0){
			llist_remove_at(l,i);
			kfree(elem);
			return SUCCESS;
		}
		head = head->next;
		i++;
	}
	return ELEM_NOT_FOUND;

}


void llist_iterate(llist * l, void (*act_on)(void *item, void * args), void * args){

	int i;
	void * elem;
	
	for(i = 0; i < l->size; i++){
		
		elem = llist_get(l,i);
		if(elem != NULL)
			act_on(elem,args);
	}
}

void llist_destroy(llist * l){

	while(l->size > 0)
		llist_remove_at(l,0);
}
