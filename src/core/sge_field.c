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
sge_encode_number(const sge_field* field, int size, const void* ud, uint8_t* buffer, field_get cb, int32_t idx) {
	long value = 0;
	uint8_t i, offset;
	sge_value sv = NEW_SGE_VALUE;
	sv.ptr = &value;
	sv.idx = idx;
	sv.name = field->name;

	cb(ud, &sv);

	for (i = 0; i < size; ++i) {
		offset = size - i - 1;
		*(buffer + i) |= (value >> (offset * 8)) & 0xff;
	}

	return size;
}

int
sge_decode_number(const sge_field* field, int size, void* ud, const uint8_t* buffer, field_set cb, int32_t idx) {
	long value = 0, v = 0xffffffffffffffff, s = 0x80;
	uint8_t i, offset;
	sge_value sv = NEW_SGE_VALUE;

	for (i = 0; i < size; ++i) {
		offset = size - i - 1;
		value |= (*(buffer + i) << (offset * 8));
	}
	v <<= (8 * size);
	s <<= (8 * (size - 1));
	if (value & s) {
		value |= v;
	}

	sv.idx = idx;
	sv.ptr = &value;
	sv.name = field->name;
	sv.vt = SGE_NUMBER;

	cb(ud, &sv);

	return size;
}

int
sge_encode_string(const sge_field* field, const void* ud, uint8_t* buffer, field_get cb, int32_t idx) {
	sge_value sv = NEW_SGE_VALUE;
	sv.idx = idx;
	sv.name = field->name;

	cb(ud, &sv);

	*buffer = (sv.len >> 8) & 0xff;
	*(buffer + 1) = sv.len & 0xff;

	if (sv.ptr) {
		memcpy(buffer + 2, sv.ptr, sv.len);
	}

	return sv.len + 2;
}

int
sge_decode_string(const sge_field* field, void* ud, const uint8_t* buffer, field_set cb, int32_t idx) {
	uint16_t len = 0;
	sge_value sv = NEW_SGE_VALUE;
	sv.idx = idx;

	len = ((*buffer << 8) & 0xff00) | (*(buffer + 1) & 0xff);
	buffer += 2;

	if (len) {
		sv.ptr = buffer;
		sv.len = len;
		sv.name = field->name;
		sv.vt = SGE_STRING;

		cb(ud, &sv);
	}

	return len + 2;
}

int
sge_encode_number_list(const sge_field* field, int size, const void* ud, uint8_t* buffer, field_get cb) {
	size_t idx = 0, offset = 0, len = 0;
	sge_value sv = NEW_SGE_VALUE;
	sv.name = field->name;

	cb(ud, &sv);

	*buffer = (sv.len >> 8) & 0xff;
	*(buffer + 1) = sv.len & 0xff;
	buffer += 2;
	len = 2;
	for (; idx < sv.len; ++idx) {
		offset = sge_encode_number(field, size, sv.ptr, buffer, cb, idx);
		buffer += offset;
		len += offset;
	}

	return len;
}

int
sge_decode_number_list(const sge_field* field, int size, void* ud, const uint8_t* buffer, field_set cb) {
	size_t len = 0, i = 0;
	size_t offset = 0, byte_len;
	sge_value sv = NEW_SGE_VALUE;

	len = ((*buffer << 8) & 0xff00) | (*(buffer + 1) & 0xff);
	buffer += 2;
	byte_len = 2;

	sv.len = len;
	sv.name = field->name;
	sv.vt = SGE_LIST;
	ud = cb(ud, &sv);

	for (; i < len; ++i) {
		offset = sge_decode_number(field, size, ud, buffer, cb, i);
		buffer += offset;
		byte_len += offset;
	}

	return byte_len;
}

int
sge_encode_string_list(const sge_field *field, const void *ud, uint8_t *buffer, field_get cb) {
	size_t idx = 0, offset = 0, len = 0;
	sge_value sv = NEW_SGE_VALUE;
	sv.name = field->name;

	cb(ud, &sv);

	*buffer = (sv.len >> 8) & 0xff;
	*(buffer + 1) = sv.len & 0xff;
	buffer += 2;
	len = 2;
	for (; idx < sv.len; ++idx) {
		offset = sge_encode_string(field, sv.ptr, buffer, cb, idx);
		buffer += offset;
		len += offset;
	}

	return len;
}

int
sge_decode_string_list(const sge_field *field, void* ud, const uint8_t* buffer, field_set cb) {
	size_t len = 0, i = 0;
	size_t offset = 0, byte_len;
	sge_value sv = NEW_SGE_VALUE;

	len = ((*buffer << 8) & 0xff00) | (*(buffer + 1) & 0xff);
	buffer += 2;
	byte_len = 2;

	sv.len = len;
	sv.name = field->name;
	sv.vt = SGE_LIST;
	ud = cb(ud, &sv);

	for (; i < len; ++i) {
		offset = sge_decode_string(field, ud, buffer, cb, i);
		buffer += offset;
		byte_len += offset;
	}

	return byte_len;
}
