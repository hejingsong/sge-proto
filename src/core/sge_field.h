#ifndef SGE_FIELD_H_
#define SGE_FIELD_H_

#include "sge_define.h"
#include "sge_list.h"

typedef struct sge_block sge_block;
typedef struct sge_field sge_field;


typedef int (*field_encode)(const sge_field*, const void*, uint8_t*, field_get cb);
typedef int (*field_decode)(const sge_field*, void*, const uint8_t*, field_set cb);
typedef void (*field_print)(const sge_field*, int level);

typedef struct {
	field_encode encode;
	field_decode decode;
	field_print print;
} sge_field_operations;

typedef struct {
	const char* name;
	size_t name_len;
	const sge_field_operations* ops;
} sge_field_type;

struct sge_field {
	sge_list head;
	const sge_field_type* type;
	sge_block* block;
	size_t name_len;
	char name[0];
};

sge_field* alloc_field(const char* name, size_t name_len, const sge_field_type* type, sge_block* block);
void destroy_field(sge_field* field);

int sge_encode_number(uint8_t* buffer, long value, int size);
int sge_decode_number(const uint8_t* buffer, long* value, int size);
int sge_encode_string(uint8_t* buffer, const char* ud, size_t len);
int sge_decode_string(const uint8_t* buffer, char** ud, size_t *len);


#endif
