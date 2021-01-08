#ifndef SGE_PARSER_H_
#define SGE_PARSER_H_

#include "sge_block.h"
#include "sge_table.h"

#define SET_ERROR(proto, ...)						\
do {												\
	int s = sprintf((proto)->err, __VA_ARGS__);		\
	(proto)->err[s] = '\0';							\
} while(0)

typedef struct {
	long len;
	long lineno;
	const char *data;
	const char *cursor;
} sge_text;

typedef struct {
	int init;
	sge_text text;
	sge_list block_head;
	sge_table *ht_name;
	sge_table *ht_idx;
	char err[1024];
} sge_proto;


int sge_parse_protocol(sge_proto* proto);
sge_field* sge_add_field(const char* field_name, size_t field_name_len, const char* type, size_t type_len);


#endif
