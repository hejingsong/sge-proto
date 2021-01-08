#include <string.h>
#include "sge_table.h"

typedef struct {
	sge_list head;
	const void* data;
	const void* key;
	size_t keylen;
} sge_item;


struct sge_table {
	sge_list slots[SLOT_SIZE];
	size_t size;
	ht_hash hash;
	ht_compare compare;
};

static sge_item*
alloc_item(const void* key, size_t len, const void* data) {
	sge_item* item = sge_malloc(sizeof(*item));
	item->data = data;
	item->key = key;
	item->keylen = len;
	LIST_INIT(&(item->head));
	return item;
}

static sge_item*
get_item(sge_list* head, const void* key, size_t len, ht_compare compare) {
	int found = 0;
	sge_list* start;
	sge_item* item;

	LIST_FOREACH(start, head) {
		item = LIST_DATA(start, sge_item, head);
		if (compare(key, item->key, len) == 0) {
			found = 1;
			break;
		}
	}
	return (found) ? item : NULL;
}

sge_table*
sge_table_alloc() {
	size_t s = sizeof(sge_table);
	sge_table *tbl = sge_malloc(s);
	memset(tbl, 0, s);
	return tbl;
}

int
sge_table_init(sge_table* tbl, ht_hash hash, ht_compare compare) {
	int i;

	tbl->size = 0;
	tbl->hash = hash;
	tbl->compare = compare;
	for (i = 0; i < SLOT_SIZE; ++i) {
		LIST_INIT(&(tbl->slots[i]));
	}
	return SGE_OK;
}

int
sge_table_insert(sge_table* tbl, const void* key, size_t len, const void* data) {
	sge_item* item = alloc_item(key, len, data);
	uint32_t idx = tbl->hash(key, len);
	sge_list* head = &(tbl->slots[idx]);
	LIST_ADD_TAIL(head, &(item->head));
	return ++tbl->size;
}

int
sge_table_remove(sge_table* tbl, const void* key, size_t len) {
	uint32_t idx = tbl->hash(key, len);
	sge_list* head = &(tbl->slots[idx]);
	sge_item* item = get_item(head, key, len, tbl->compare);
	if (item == NULL) {
		return SGE_OK;
	}
	LIST_REMOVE(&(item->head));
	sge_free(item);
	return --tbl->size;
}

void*
sge_table_get(sge_table* tbl, const void* key, size_t len) {
	uint32_t idx = tbl->hash(key, len);
	sge_list* head = &(tbl->slots[idx]);
	sge_item* item = get_item(head, key, len, tbl->compare);
	return (item) ? (void*)item->data : NULL;
}

void
sge_table_destroy(sge_table* tbl) {
	int i;
	sge_list* head, *cur, *next;
	sge_item* item;

	for (i = 0; i < SLOT_SIZE; ++i) {
		head = &(tbl->slots[i]);
		cur = head->next;
		while(!LIST_EMPTY(head)) {
			next = cur->next;
			item = LIST_DATA(cur, sge_item, head);
			LIST_REMOVE(cur);
			sge_free(item);
			cur = next;
		}
	}
}

