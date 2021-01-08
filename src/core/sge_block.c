#include <string.h>
#include "sge_block.h"

sge_block*
sge_alloc_block(const char* block_name, size_t name_len, uint32_t idx) {
	size_t size = sizeof(sge_block) + name_len + 1;
	sge_block* block = sge_malloc(size);
	block->idx = idx;
	LIST_INIT(&(block->head));
	LIST_INIT(&(block->field_head));
	block->size = 0;
	strncpy(block->name, block_name, name_len);
	block->name[name_len] = '\0';
	return block;
}

void
sge_destroy_block(sge_block* block) {
	sge_list* p, *next;
	sge_field* field;
	sge_list* head = &block->field_head;

	for (p = head->next; !LIST_EMPTY(head); ) {
		next = p->next;
		field = LIST_DATA(p, sge_field, head);
		destroy_field(field);
		p = next;
	}

	LIST_REMOVE(&block->head);
	sge_free(block);
}
