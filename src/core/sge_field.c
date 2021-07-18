#include <string.h>
#include "sge_field.h"


sge_field*
alloc_field(const char* name, size_t name_len, const sge_field_type* type, sge_block* block) {
	size_t size = sizeof(sge_field) + name_len + 1;
	sge_field* field = sge_malloc(size);
	field->type = type;
	field->block = block;
	field->name_len = name_len;
	LIST_INIT(&(field->head));
	strncpy(field->name, name, name_len);
	field->name[name_len] = '\0';
	return field;
}

void
destroy_field(sge_field* field) {
	LIST_REMOVE(&(field->head));
	sge_free(field);
}

int
sge_encode_number(uint8_t* buffer, long value, int size) {
	int i, offset;

	for (i = 0; i < size; ++i) {
		offset = size - i - 1;
		*(buffer + i) |= (value >> (offset * 8)) & 0xff;
	}

	return size;
}

int
sge_decode_number(const uint8_t* buffer, long* value, int size) {
	long val = 0, v = 0xffffffffffffffff, s = 0x80;
	int i, offset;

	for (i = 0; i < size; ++i) {
		offset = size - i - 1;
		val |= (*(buffer + i) << (offset * 8));
	}
	v <<= (8 * size);
	s <<= (8 * (size - 1));
	if (val & s) {
		val |= v;
	}

	*value = val;
	return size;
}

int
sge_encode_string(uint8_t* buffer, const char* ud, size_t len) {
	sge_encode_number(buffer, len, 2);

	if (ud) {
		memcpy(buffer + 2, ud, len);
	}
	return len + 2;
}

int sge_decode_string(const uint8_t* buffer, char** ud, size_t *len) {
	long value;
	int sz;

	sz = sge_decode_number(buffer, &value, 2);
	*ud = (char*)buffer + sz;
	*len = value;
	return sz + 2;
}
