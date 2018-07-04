#include "hashmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int default_hash(void * elem){
	int * ptr = (int *) elem;
	return *ptr;
}

int default_key_comparer(void * key1, void * key2){
	

	if(key1 == key2)
		return 0;
	else
		return 1;
}


int int_key_comparer(int * key1, int * key2){
	
	if(key1 == NULL || key2 == NULL) {
		return 1;
	}
	
	if(*key1 == *key2)
		return 0;
	else
		return 1;
	
}

int str_key_comparer(char * key1, char * key2){

	return strcmp(key1,key2);
}



int str_hash(char * s)
{

    int hash = 0;
    char * ptr = s;
    int i = 0;
    while(*ptr != NULL)
    {
    	hash += *ptr;
    	hash += (hash << 10);
    	hash ^= (hash >> 6);

        ++ptr;
    }

    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);


    return hash;
}

int int_hash(int * val){

	char buffer[33];
	int hash;
	memset(buffer,0,33);
	sprintf(buffer,"%d", *val);
	hash = str_hash(buffer);
	return hash;

}

int hmap_elem_comparer(hashmap_elem * elem1, hashmap_elem * elem2){

	if(elem1 != NULL && elem2 != NULL) {
		return elem1->equals(elem1->key, elem2->key);
	}
	else {
		return 1;
	}

}

void hmap_init(hashmap * h, int m_size){
	
	int i = 0;
	int map_size;

	if(m_size == 0)
		map_size = DEFAULT_MAP_SIZE;
	else
		map_size = m_size;
	
	h->size = map_size;
	h->head = malloc(sizeof(llist *) * map_size);
	for(i = 0; i < map_size; i++){
		h->head[i] = (llist *) malloc(sizeof(llist));
		llist_init(h->head[i]);
	}



	for(i = 0; i < h->size; i++){
		llist_set_equality_checker(h->head[i],hmap_elem_comparer);
	}
	
	
	h->hash = int_hash;
	h->key_comparer = int_key_comparer;
	
}

void hmap_set_hash(hashmap * h, int (*hashfn) (void * item)){
	h->hash = hashfn;
}

void hmap_set_comparer(hashmap * h, int (*comparerfn) (void * elem1, void * elem2)){
	h->key_comparer = comparerfn;
}


void hmap_put_abs(hashmap * h, int key, void * value){
	int index;
	hashmap_elem * new_elem;
	hashmap_elem * temp;

	if(h == NULL)
		return;

	
	
	index = key % h->size;
	llist * list;	
	list = h->head[index];
	llist_elem * head;

	new_elem = (hashmap_elem *) malloc(sizeof(hashmap_elem));
	//new_elem->key = NULL;
	new_elem->key_val = key;
	new_elem->value = value;
	new_elem->equals = NULL;
	head = list->head;
	while(head != NULL){
		temp = (hashmap_elem *) head->item;
		if(temp->key_val == key) {
			temp->value = value;
			free(new_elem);
			return;
		}
		head = head->next;
	}

	llist_append(list,new_elem);

}

void* hmap_get_abs(hashmap * h, int key){
	
	int index;
	hashmap_elem new_elem;	
	hashmap_elem * temp;

	if(h == NULL)
		return NULL;

	index = (key) % h->size;
	
	llist * list;	
	list = h->head[index];
	llist_elem * head = list->head;

	while(head != NULL){
		temp = (hashmap_elem *) head->item;
		if(temp == NULL){
			head = head->next;
			//printk(KERN_INFO "HMAP: Item is NULL. Key = %d\n", key);
			continue;
		}
		if(temp->key_val ==key) {
			//if(temp->value == NULL)
			//	printk(KERN_INFO "HMAP: Value exists but is NULL\n");
			return temp->value;
		}
		head = head->next;
	}

	return NULL;

}


void hmap_remove_abs(hashmap * h, int key){

	int index,i;
	hashmap_elem * temp;
		
	index = (key) % h->size;
	llist * list;	
	list = h->head[index];
	llist_elem * head = list->head;
	
	if(h == NULL)
		return;

	i = 0;
	while(head != NULL){
		temp = (hashmap_elem *) head->item;
		if(temp->key_val == key) {
			temp->key_val = 0;
			return;			
		}
		i++;
		head = head->next;
	}
}



void hmap_destroy(hashmap * h){

	int i;
	llist * l;
	llist_elem * head;
	llist_elem * temp;
	for(i = 0; i < h->size; i++){
		l = h->head[i];
		head = l->head;
		while(head != NULL){
			temp = head;
			head = head->next;
			free(temp->item);
			free(temp);
		}
		free(h->head[i]);
	}
	free(h->head);
	

}


