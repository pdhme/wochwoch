#ifndef d4f6ea_HASH_TABLE
#define d4f6ea_HASH_TABLE

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <math.h>
#include <pthread.h>


typedef struct {
	char* key;
	void* value;
	uint8_t free : 1;
} htel;

typedef struct {
	size_t size;
	size_t ea;
	htel** e;
	char resize;
	pthread_mutex_t mutex;
} hash_table;

extern htel DELETED_ITEM;

static short is_prime(const int);
static int next_prime(int);
static size_t hash(const char*, const int, const int);

hash_table* hash_table_init(size_t);
short ht_set(hash_table*, char*, void*);
short ht_set_str(hash_table*, char*, char*);
short ht_set_heap(hash_table* ht, char* key, void* value);
void* ht_get_all(hash_table*, size_t, ...);
void* ht_get(hash_table*, char*);
void* ht_nget(hash_table*, char*, size_t);
short ht_rm(hash_table*, char*);
short ht_resize(hash_table**, const size_t);

static void ef(htel*);
static void htef(hash_table*);
void hash_table_free(hash_table*);
void print_hash_table(hash_table*, char);

#endif
