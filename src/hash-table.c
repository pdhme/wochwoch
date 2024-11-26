#include "hash-table.h"
#define sfree(p) {free(p);p=NULL;}

htel DELETED_ITEM = {NULL, NULL};

static short is_prime(const int x) {
	if (x < 2 || x % 2 == 0) return 0;
	for (int i = 3; i <= floor(sqrt((double) x)); i += 2) if (x % i == 0) return 0;
	return 1;
}

// Return the next prime after x, or x if x is prime
static int next_prime(int x) {
	while (!is_prime(x)) x++;
	return x;
}

static size_t nhash(const char* str, const int m, const int a, const size_t len) {
	size_t h = 0;
	for (size_t i = 0; i < len; i++) h = ( h + str[i] * (size_t) pow(a, len - i-1) ) % m;
	return h;
}

static int hstrncmp(char* s1, char* s2, size_t len) {
	while (*s1 && *s2 && len) {
		if (*s1 != *s2)
			return (unsigned char)(*s1) - (unsigned char)(*s2);
		s1++; s2++; len--;
	}
	return 0;
}

static size_t hash(const char* str, const int m, const int a) {
	return nhash(str, m, a, strlen(str));
}

hash_table* hash_table_init(size_t size) {
	hash_table* t;
	if (size < 1) return NULL;
	if ( (t = malloc(sizeof(hash_table))) == NULL) return NULL;

	t->size = size;
	t->ea = 0;
	t->resize = 1;
	if ( (t->e = calloc(t->size, sizeof(htel*))) == NULL) return NULL;

	pthread_mutex_init(&t->mutex, NULL);
	return t;
}

// Add key-value pair to hash table
short ht__set(hash_table* ht, char* key, void* value, uint8_t free_value) {
	int err = 0;
	htel* e;
	if (ht == NULL || key == NULL) goto arg_err;

	if ( (e = malloc(sizeof(htel))) == NULL) goto malloc_err;
	if ( (e->key = strdup(key)) == NULL) goto strdup_err;
	e->value = value;
	e->free = free_value ? 1 : 0;

	int a = 0;
	size_t i = -1;
	size_t h = hash(key, ht->size, 151);
	size_t hh= hash(key, ht->size, 149) + 1;

	pthread_mutex_lock(&ht->mutex);
	const int load = ht->ea * 100 / ht->size;
	if (ht->resize)
	if (load > 75)
		if (ht_resize(&ht, ht->size * 2) < 0) goto resize_err;

	while (1) {
		i = h % ht->size;
		if (ht->e[i] == NULL || ht->e[i] == &DELETED_ITEM) {
			ht->e[i] = e;
			ht->ea++;
			goto quit;
		}
		if (strcmp(ht->e[i]->key, key) == 0) {
			if (ht->e[i]->free)
				sfree(ht->e[i]->value);
			ht->e[i]->value = value;
			goto quit;
		}
		h += hh;
	}

	// Simple error handling
	err: err++;
	resize_err: err++;
				sfree(e->key);
	strdup_err: err++;
				sfree(e);
	malloc_err: err++;
	arg_err: err++;
	quit:
	pthread_mutex_unlock(&ht->mutex);
	return err;
}

short ht_set(hash_table* ht, char* key, void* value) {
	return ht__set(ht, key, value, 0);
}

short ht_set_heap(hash_table* ht, char* key, void* value) {
	return ht__set(ht, key, value, 1);
}

short ht_set_str(hash_table* ht, char* key, char* value) {
	if (value == NULL) return 100;
	char* v = strdup(value);
	if (v == NULL) return -5;
	return ht__set(ht, key, (void*) v, 1);
}

short ht_resize(hash_table** ht, const size_t size) {
	int err = 0;
	if (ht == NULL || size < 1) goto arg_err;

	hash_table* new_ht = hash_table_init(size);
	if (new_ht == NULL) goto init_err;
	pthread_mutex_lock(&(*ht)->mutex);
	for (size_t i = 0; i < (*ht)->size; i++) {
		htel* e = (*ht)->e[i];
		if (e != NULL && e != &DELETED_ITEM) 
			if ( ht_set(new_ht, e->key, e->value) < 0) goto add_err;
	}
	htef(*ht);
	(*ht)->size = new_ht->size;
	(*ht)->ea = new_ht->ea;
	(*ht)->e = new_ht->e;
	pthread_mutex_unlock(&(*ht)->mutex);
	return 0;

	// Simple error handling
	add_err: err++;
	hash_table_free(new_ht);
	init_err: err++;
	arg_err: err++;
	return err;
}

void* ht_get_all(hash_table* ht, size_t n, ...) {
	va_list args;
	void* value;
	char* key;
	va_start(args, n);
	for (size_t i = 0; i < n; i++) {
		key = va_arg(args, char*);
		if ( (value = ht_get(ht, key)) != NULL) goto found;
	}
	return NULL;
	found:
	va_end(args);
	return value;
}

// Get key-value pair from hash table
void* ht_get(hash_table* ht, char* key) {
	if (ht == NULL || key == NULL) return NULL;
	return ht_nget(ht, key, strlen(key));
}

void* ht_nget(hash_table* ht, char* key, size_t len) {
	int a = 0;
	size_t i = -1;
	size_t h = nhash(key, ht->size, 151, len);
	size_t hh= nhash(key, ht->size, 149, len) + 1;
	void* value = NULL;
	if (ht == NULL || key == NULL) return NULL;

	pthread_mutex_lock(&ht->mutex);
	while (1) {
		i = h % ht->size;
		value = NULL;
		if (ht->e[i] == NULL) break;
		if (ht->e[i] == &DELETED_ITEM) continue;
		value = ht->e[i]->value;
		if (!hstrncmp(ht->e[i]->key, key, len)) break;
		h += hh;
	}
	pthread_mutex_unlock(&ht->mutex);

	return value;
}

// Remove key-value pair from hash table
short ht_rm(hash_table* ht, char* key) {
	int a = 0;
	size_t i = -1;
	size_t h = hash(key, ht->size, 151);
	size_t hh= hash(key, ht->size, 149) + 1;
	int err = 0;
	if (ht == NULL || key == NULL) goto arg_err;

	const int load = ht->ea * 100 / ht->size;
	if (ht->resize)
	if (load < 25)
		if (ht_resize(&ht, ht->size / 2) < 0)
			goto resize_err;

	while (1) {
		i = h % ht->size;
		if (ht->e[i] == NULL) goto element_err;
		if (ht->e[i] != &DELETED_ITEM) 
			if (strcmp(ht->e[i]->key, key) == 0) {
				ef(ht->e[i]);
				ht->e[i] = &DELETED_ITEM;
				ht->ea--;
				goto quit;
			}
		h += hh;
	}

	// Simple error handling
	element_err: err++;
	resize_err: err++;
	arg_err: err++;
	quit:
	return err;
}

static void ef(htel* e) {
	if (e == NULL || e == &DELETED_ITEM) return;

	if (e->free) sfree(e->value);
	sfree(e->key);
	sfree(e);
}

static void htef(hash_table* ht) {
	if (ht == NULL) return;
	for (size_t i = 0; i < ht->size; i++)
		ef(ht->e[i]);
	sfree(ht->e);
}

void hash_table_free(hash_table* ht) {
	if (!ht) return;
	pthread_mutex_destroy(&ht->mutex);
	htef(ht);
	sfree(ht);
}

void print_hash_table(hash_table* ht, char print_value) {
	if (ht == NULL) return;
	printf("Hash table size: %zu\nElement count: %zu\n", ht->size, ht->ea);
	for (size_t i = 0; i < ht->size; i++)
		if (ht->e[i] != NULL && ht->e[i] != &DELETED_ITEM)
			if (print_value)
				printf("[%zu] %s:\t\t%s\n", i, ht->e[i]->key, ht->e[i]->value);
			else
				printf("[%zu] %s:\n", i, ht->e[i]->key);
}
