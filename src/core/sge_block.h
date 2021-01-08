#ifndef SGE_BLOCK_H_
#define SGE_BLOCK_H_

#include "sge_field.h"

struct sge_block {
	sge_list head;
	uint32_t idx;
	uint32_t size;
	sge_list field_head;
	char name[0];
};

sge_block* sge_alloc_block(const char* block_name, size_t name_len, uint32_t idx);
void sge_destroy_block(sge_block* block);

#endif
