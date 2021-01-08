#ifndef SGE_TABLE_H_
#define SGE_TABLE_H_

#include "sge_define.h"
#include "sge_list.h"

#define SLOT_SIZE 24

typedef uint32_t (*ht_hash)(const void*, size_t);
typedef int (*ht_compare)(const void*, const void*, size_t);

typedef struct sge_table sge_table;

sge_table* sge_table_alloc();
int sge_table_init(sge_table* tbl, ht_hash hash, ht_compare compare);
int sge_table_insert(sge_table* tbl, const void* key, size_t len, const void* data);
int sge_table_remove(sge_table* tbl, const void* key, size_t len);
void* sge_table_get(sge_table* tbl, const void* key, size_t len);
void sge_table_destroy(sge_table* tbl);


#endif
