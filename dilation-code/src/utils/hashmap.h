#ifndef __HASHMAP_H
#define __HASHMAP_H

#include "linkedlist.h"
#include <linux/spinlock_types.h>

#define DEFAULT_MAP_SIZE 1000

typedef int (*hashFn)(void * item);
typedef int (*equality_checker_fn)(void * key1, void * key2);

typedef struct hashmap_elem_struct{

	void * key;
	int key_val;
	void * value;
	equality_checker_fn equals;
	
}
hashmap_elem;

typedef struct hashmap_struct{

	llist ** head;
	int size;
	hashFn hash;
	equality_checker_fn key_comparer;

}
hashmap;

void hmap_init(hashmap * h, char * type , int m_size);
void hmap_set_hash(hashmap * h, int (*hashfn) (void * item));
void hmap_set_comparer(hashmap * h, int (*comparerfn) (void * elem1, void * elem2));
void hmap_put(hashmap * h, void * key, void * value);
void* hmap_get(hashmap * h, void * key);
void hmap_remove(hashmap * h, void * key);
void hmap_destroy(hashmap * h);
int int_hash(int * val);
int str_hash(char * s);

void hmap_put_abs(hashmap * h, int key, void * value);
void* hmap_get_abs(hashmap * h, int key);
void hmap_remove_abs(hashmap * h, int key);


#endif
