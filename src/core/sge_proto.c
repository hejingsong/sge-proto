#include <stdio.h>
#include <string.h>

#include "sge_proto.h"
#include "sge_block.h"
#include "sge_parser.h"
#include "sge_crc16.h"

#define PACK_UNIT_SIZE 8

static const char *SGE_PROTOCOL_HEADER = "01";
static const uint8_t SGE_PROTOCOL_HEADER_SIZE = 2;

static int
sge_get_number(const void* ud, field_get_fn cb, const char* field_name, long* value, int idx) {
	long val = 0;
	sge_value sv = NEW_SGE_VALUE;
	sv.ptr = &val;
	sv.idx = idx;
	sv.name = field_name;

	cb(ud, &sv);
	*value = val;
	return SGE_OK;
}

static int
sge_set_number(void* ud, field_set_fn cb, const char* field_name, long value, int idx) {
	sge_value sv = NEW_SGE_VALUE;
	sv.idx = idx;
	sv.ptr = &value;
	sv.name = field_name;
	sv.vt = SGE_NUMBER;

	cb(ud, &sv);
	return SGE_OK;
}

static int
sge_encode_block(const sge_block* block, const void* ud, uint8_t* buffer, field_get cb) {
	sge_field *field;
	sge_list* pf;
	size_t offset = 0;
	const uint8_t *start = buffer;

	LIST_FOREACH(pf, &block->field_head) {
		field = LIST_DATA(pf, sge_field, head);
		offset = field->type->ops->encode(field, ud, buffer, cb);
		buffer += offset;
	}

	return buffer - start;
}

static int
sge_decode_block(const sge_block *block, void *ud, const uint8_t *buffer, field_set cb) {
	sge_field *field;
	sge_list* pf;
	size_t offset = 0;
	const uint8_t *start = buffer;

	LIST_FOREACH(pf, &block->field_head) {
		field = LIST_DATA(pf, sge_field, head);
		offset = field->type->ops->decode(field, ud, buffer, cb);
		buffer += offset;
	}

	return buffer - start;
}

static int
sge_encode_dict(const sge_field *field, const void *ud, uint8_t *buffer, field_get cb, int32_t idx) {
	uint8_t *start = buffer;
	size_t offset = 0;
	sge_value sv = NEW_SGE_VALUE;
	sv.idx = idx;
	sv.name = field->name;

	cb(ud, &sv);

	if (sv.ptr) {
		*buffer = (sv.len & 0xff);
		buffer++;
		offset = sge_encode_block(field->block, sv.ptr, buffer, cb);
		buffer += offset;
	} else {
		*buffer = 0;
		buffer++;
	}

	return buffer - start;
}

static int
sge_decode_dict(const sge_field* field, void* ud, const uint8_t* buffer, field_set cb, int32_t idx) {
	sge_value sv = NEW_SGE_VALUE;
	uint8_t len = 0;

	len = (uint8_t)*buffer;
	if (len == 0) {
		return 1;
	}

	sv.name = field->name;
	sv.vt = SGE_DICT;
	sv.idx = idx;
	ud = cb(ud, &sv);

	return sge_decode_block(field->block, ud, buffer + 1, cb) + 1;
}

static int
encode_number(const sge_field* field, const void* ud, uint8_t* buffer, field_get_fn cb, int size, int idx) {
	long value = 0;
	sge_get_number(ud, cb, field->name, &value, idx);
	return sge_encode_number(buffer, value, size);
}

static int
decode_number(const sge_field* field, void* ud, const uint8_t* buffer, field_set cb, int size, int idx) {
	int ret;
	long value = 0;

	ret = sge_decode_number(buffer, &value, size);
	sge_set_number(ud, cb, field->name, value, idx);
	return ret;
}

static int
encode_number_list(const sge_field* field, const void* ud, uint8_t* buffer, field_get cb, int size) {
	size_t idx = 0, offset = 0, len = 0;
	sge_value sv = NEW_SGE_VALUE;
	sv.name = field->name;

	cb(ud, &sv);
	len = sge_encode_number(buffer, sv.len, 2);
	buffer += len;
	for (; idx < sv.len; ++idx) {
		offset = encode_number(field, sv.ptr, buffer, cb, size, idx);
		buffer += offset;
		len += offset;
	}

	return len;
}

static int
decode_number_list(const sge_field* field, void* ud, const uint8_t* buffer, field_set cb, int size) {
	size_t len = 0, i = 0;
	size_t offset = 0, byte_len;
	sge_value sv = NEW_SGE_VALUE;

	byte_len = sge_decode_number(buffer, (long*)&len, 2);
	buffer += byte_len;

	sv.len = len;
	sv.name = field->name;
	sv.vt = SGE_LIST;
	ud = cb(ud, &sv);
	for (; i < len; ++i) {
		offset = decode_number(field, ud, buffer, cb, size, i);
		buffer += offset;
		byte_len += offset;
	}

	return byte_len;
}

static int
encode_string_ex(const sge_field* field, const void* ud, uint8_t* buffer, field_get cb, int idx) {
	sge_value sv = NEW_SGE_VALUE;
	sv.idx = idx;
	sv.name = field->name;

	cb(ud, &sv);
	return sge_encode_string(buffer, sv.ptr, sv.len);
}

static int
decode_string_ex(const sge_field* field, void* ud, const uint8_t* buffer, field_set cb, int idx) {
	size_t len = 0;
	char* ptr;
	sge_value sv = NEW_SGE_VALUE;
	sv.idx = idx;

	sge_decode_string(buffer, &ptr, &len);
	if (len) {
		sv.ptr = ptr;
		sv.len = len;
		sv.name = field->name;
		sv.vt = SGE_STRING;
		cb(ud, &sv);
	}

	return len + 2;
}

static int
encode_number8(const sge_field* field, const void* ud, uint8_t* buffer, field_get cb) {
	return encode_number(field, ud, buffer, cb, 1, -1);
}

static int
decode_number8(const sge_field* field, void* ud, const uint8_t* buffer, field_set cb) {
	return decode_number(field, ud, buffer, cb, 1, -1);
}

static int
encode_number16(const sge_field* field, const void* ud, uint8_t* buffer, field_get cb) {
	return encode_number(field, ud, buffer, cb, 2, -1);
}

static int
decode_number16(const sge_field* field, void* ud, const uint8_t* buffer, field_set cb) {
	return decode_number(field, ud, buffer, cb, 2, -1);
}

static int
encode_number32(const sge_field* field, const void* ud, uint8_t* buffer, field_get cb) {
	return encode_number(field, ud, buffer, cb, 4, -1);
}

static int
decode_number32(const sge_field* field, void* ud, const uint8_t* buffer, field_set cb) {
	return decode_number(field, ud, buffer, cb, 4, -1);
}

static int
encode_number8_list(const sge_field* field, const void* ud, uint8_t* buffer, field_get cb) {
	return encode_number_list(field, ud, buffer, cb, 1);
}

static int
decode_number8_list(const sge_field* field, void* ud, const uint8_t* buffer, field_set cb) {
	return decode_number_list(field, ud, buffer, cb, 1);
}

static int
encode_number16_list(const sge_field* field, const void* ud, uint8_t* buffer, field_get cb) {
	return encode_number_list(field, ud, buffer, cb, 2);
}

static int
decode_number16_list(const sge_field* field, void* ud, const uint8_t* buffer, field_set cb) {
	return decode_number_list(field, ud, buffer, cb, 2);
}

static int
encode_number32_list(const sge_field* field, const void* ud, uint8_t* buffer, field_get cb) {
	return encode_number_list(field, ud, buffer, cb, 4);
}

static int
decode_number32_list(const sge_field* field, void* ud, const uint8_t* buffer, field_set cb) {
	return decode_number_list(field, ud, buffer, cb, 4);
}

static int
encode_string(const sge_field* field, const void* ud, uint8_t* buffer, field_get cb) {
	return encode_string_ex(field, ud, buffer, cb, -1);
}

static int
decode_string(const sge_field* field, void* ud, const uint8_t* buffer, field_set cb) {
	return decode_string_ex(field, ud, buffer, cb, -1);
}

static int
encode_string_list(const sge_field* field, const void* ud, uint8_t* buffer, field_get cb) {
	size_t idx = 0, offset = 0, len = 0;
	sge_value sv = NEW_SGE_VALUE;
	sv.name = field->name;

	cb(ud, &sv);

	len = sge_encode_number(buffer, sv.len, 2);
	buffer += len;
	for (; idx < sv.len; ++idx) {
		offset = encode_string_ex(field, sv.ptr, buffer, cb, idx);
		buffer += offset;
		len += offset;
	}

	return len;
}

static int
decode_string_list(const sge_field* field, void* ud, const uint8_t* buffer, field_set cb) {
	size_t len = 0, i = 0;
	size_t offset = 0, byte_len;
	sge_value sv = NEW_SGE_VALUE;

	byte_len = sge_decode_number(buffer, (long*)&len, 2);
	buffer += byte_len;

	sv.len = len;
	sv.name = field->name;
	sv.vt = SGE_LIST;
	ud = cb(ud, &sv);
	for (; i < len; ++i) {
		offset = decode_string_ex(field, ud, buffer, cb, i);
		buffer += offset;
		byte_len += offset;
	}

	return byte_len;
}

static int
encode_dict(const sge_field* field, const void* ud, uint8_t* buffer, field_get cb) {
	return sge_encode_dict(field, ud, buffer, cb, -1);
}

static int
decode_dict(const sge_field* field, void* ud, const uint8_t* buffer, field_set cb) {
	return sge_decode_dict(field, ud, buffer, cb, -1);
}

static int
encode_dict_list(const sge_field* field, const void* ud, uint8_t* buffer, field_get cb) {
	size_t idx = 0, offset = 0;
	sge_value sv = NEW_SGE_VALUE;
	uint8_t *start = buffer;

	sv.name = field->name;
	cb(ud, &sv);

	*buffer = (sv.len >> 8) & 0xff;
	*(buffer + 1) = sv.len & 0xff;
	buffer += 2;

	for (; idx < sv.len; ++idx) {
		offset = sge_encode_dict(field, sv.ptr, buffer, cb, idx);
		buffer += offset;
	}

	return buffer - start;
}

static int
decode_dict_list(const sge_field* field, void* ud, const uint8_t* buffer, field_set cb) {
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
		offset = sge_decode_dict(field, ud, buffer, cb, i);
		buffer += offset;
		byte_len += offset;
	}

	return byte_len;
}

static void
print_field(const sge_field* field, int level) {
	printf("%*.sname: %s, type: %s\n", level * 4, " ", field->name, field->type->name);
}

static void
print_custom_field(const sge_field* field, int level) {
	char type[32];
	snprintf(type, 32, field->type->name, field->block->name);
	printf("%*.sname: %s, type: %s\n", level * 4, " ", field->name, type);
}

static const sge_field_operations number8_ops = {
	.encode=encode_number8,
	.decode=decode_number8,
	.print=print_field
};

static const sge_field_operations number16_ops = {
	.encode=encode_number16,
	.decode=decode_number16,
	.print=print_field
};

static const sge_field_operations number32_ops = {
	.encode=encode_number32,
	.decode=decode_number32,
	.print=print_field
};

static const sge_field_operations number8_list_ops = {
	.encode=encode_number8_list,
	.decode=decode_number8_list,
	.print=print_field
};

static const sge_field_operations number16_list_ops = {
	.encode=encode_number16_list,
	.decode=decode_number16_list,
	.print=print_field
};

static const sge_field_operations number32_list_ops = {
	.encode=encode_number32_list,
	.decode=decode_number32_list,
	.print=print_field
};

static const sge_field_operations string_ops = {
	.encode=encode_string,
	.decode=decode_string,
	.print=print_field
};

static const sge_field_operations string_list_ops = {
	.encode=encode_string_list,
	.decode=decode_string_list,
	.print=print_field
};

static const sge_field_operations custom_ops = {
	.encode=encode_dict,
	.decode=decode_dict,
	.print=print_custom_field
};

static const sge_field_operations custom_list_ops = {
	.encode=encode_dict_list,
	.decode=decode_dict_list,
	.print=print_custom_field
};

static const sge_field_type field_type_table[] = {
	{"number", 6, &number32_ops},
	{"number[]", 8, &number32_list_ops},
	{"number8", 7, &number8_ops},
	{"number16", 8, &number16_ops},
	{"number32", 8, &number32_ops},
	{"number8[]", 9, &number8_list_ops},
	{"number16[]", 10, &number16_list_ops},
	{"number32[]", 10, &number32_list_ops},
	{"string", 6, &string_ops},
	{"string[]", 8, &string_list_ops},
	{NULL, 0, NULL},
	{"%s", 2, &custom_ops},
	{"%s[]", 4, &custom_list_ops},
};

static sge_proto protocol = {
	.init=0
};

static uint32_t
hash_string(const void* s, size_t s_len) {
	char* data = (char*)s;
	size_t i = 0;
	size_t len = strlen(data);
	unsigned long hash = 5381;
	len = (len > s_len) ? s_len : len;

	for (; i < len; ++i) {
		hash = ((hash << 5) + hash) + *data++;
	}

	return hash % SLOT_SIZE;
}

static uint32_t
hash_number(const void* d, size_t len) {
	return *(uint32_t*)d % SLOT_SIZE;
}

static int
compare_string(const void* ptr, const void* key, size_t keylen) {
	return strncmp(ptr, key, keylen);
}

static int
compare_number(const void* ptr, const void* key, size_t keylen) {
	uint32_t i = *(uint32_t*)key;
	uint32_t j = *(uint32_t*)ptr;
	return i != j;
}


static int
init_protocol() {
	protocol.ht_idx = sge_table_alloc();
	protocol.ht_name = sge_table_alloc();
	LIST_INIT(&(protocol.block_head));
	LIST_INIT(&(protocol.unfinished_fields));
	sge_table_init(protocol.ht_idx, hash_number, compare_number);
	sge_table_init(protocol.ht_name, hash_string, compare_string);
	memset(protocol.err, 0, sizeof(protocol.err));
	protocol.init = 1;
	return SGE_OK;
}

static int
parse_text(const char* text) {
	if (protocol.init == 0) {
		init_protocol();
	}
	protocol.text.data = text;
	protocol.text.len = strlen(text);
	protocol.text.cursor = text;
	protocol.text.lineno = 1;

	return sge_parse_protocol(&protocol);
}

int
sge_get_block(const char* type, size_t type_len, sge_block** block) {
	int ret = 1;
	size_t cmp_type_len;

	cmp_type_len = type_len;
	if (0 == strncmp("[]", type + type_len - 2, 2)) {
		cmp_type_len -= 2;
		ret = 2;
	}

	*block = (sge_block*)sge_table_get(protocol.ht_name, type, cmp_type_len);
	return ret;
}

int
sge_add_field(const char* field_name, size_t field_name_len, const char* type, size_t type_len, sge_field** field) {
	const sge_field_type* field_type = NULL;
	const sge_field_type* p = field_type_table;
	sge_block* block = NULL;
	size_t cmp_type_len;
	int offset = 0;
	int ret = SGE_OK;

	for (; p->name != NULL; ++p) {
		cmp_type_len = (type_len >= p->name_len) ? type_len : p->name_len;
		if (strncmp(p->name, type, cmp_type_len) == 0) {
			field_type = p;
			break;
		}
	}
	if (NULL == field_type) {
		offset = sge_get_block(type, type_len, &block);
		field_type = p + offset;
		if (!block) {
			ret = SGE_ERR;
		}
	}

	*field = alloc_field(field_name, field_name_len, field_type, block);
	return ret;
}


// export
int
sge_parse(const char* text) {
	int ret;
	if (NULL == text) {
		return INVALID_PARAM;
	}
	ret = parse_text(text);
	if (SGE_OK != ret) {
		sge_destroy(0);
	}
	return ret;
}

int
sge_parse_file(const char* file) {
	int ret;
	long len = 0;
	FILE *fp = NULL;
	char *buffer = NULL;

	if (NULL == file) {
		return INVALID_PARAM;
	}

	fp = fopen(file, "rb");
	if (NULL == fp) {
		return RES_CANT_ACCESS;
	}

	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	buffer = sge_malloc(len + 1);

	fread(buffer, len, 1, fp);
	buffer[len] = '\0';
	fclose(fp);

	ret = parse_text(buffer);
	sge_free(buffer);
	if (SGE_OK != ret) {
		sge_destroy(0);
	}
	return ret;
}

int
sge_encode(const char* name, const void *ud, char* buffer, field_get cb) {
	sge_block *block;
	uint16_t crc;
	uint32_t keylen;
	size_t offset = 0;
	uint8_t *p_buffer = (uint8_t *)buffer;

	if (NULL == name || NULL == ud || NULL == buffer || NULL == cb) {
		return INVALID_PARAM;
	}

	if (protocol.init == 0) {
		return NOT_SCHEME;
	}

	keylen = strlen(name);
	block = (sge_block*)sge_table_get(protocol.ht_name, name, keylen);
	if (NULL == block) {
		SET_ERROR(&protocol, "can't found protocol: %s", name);
		return SGE_ERR;
	}

	p_buffer += 2;
	memcpy(p_buffer, SGE_PROTOCOL_HEADER, SGE_PROTOCOL_HEADER_SIZE);
	p_buffer += 2;
	p_buffer += sge_encode_number(p_buffer, block->idx, 2);
	offset = sge_encode_block(block, ud, p_buffer, cb);
	crc = sge_crc16(buffer + 2, offset + 4);
	sge_encode_number((uint8_t*)buffer, crc, 2);
	return offset + 6;
}

int
sge_decode(const char* buffer, void* ud, field_set cb) {
	uint32_t proto_idx;
	uint16_t s_crc, d_crc;
	size_t byte_len;
	sge_block *block = NULL;
	long l_proto_idx, l_s_crc;
	const uint8_t *p = (uint8_t *)buffer;

	if (NULL == buffer || NULL == ud || NULL == cb) {
		return INVALID_PARAM;
	}

	if (protocol.init == 0) {
		return NOT_SCHEME;
	}

	p += sge_decode_number(p, &l_s_crc, 2);
	s_crc = (uint16_t)l_s_crc;

	if (memcmp(p, SGE_PROTOCOL_HEADER, SGE_PROTOCOL_HEADER_SIZE) != 0) {
		SET_ERROR(&protocol, "bytes wrong format.");
		return SGE_ERR;
	}
	
	p += SGE_PROTOCOL_HEADER_SIZE;
	sge_decode_number(p, &l_proto_idx, 2);
	proto_idx = (uint32_t)l_proto_idx;
	p += 2;

	block = (sge_block*)sge_table_get(protocol.ht_idx, (void*)&proto_idx, -1);
	if (NULL == block) {
		SET_ERROR(&protocol, "can't found protocol: %d", proto_idx);
		return SGE_ERR;
	}
	byte_len = sge_decode_block(block, ud, p, cb);
	d_crc = sge_crc16(buffer + 2, byte_len + 4);
	if (s_crc != d_crc) {
		SET_ERROR(&protocol, "invalid protocol");
		return SGE_ERR;
	}

	return proto_idx;
}

int
sge_pack(const char* in_str, int len, char* out_str) {
	int move_step = 0;
	char* mask = out_str;
	char* p_out = out_str + 1;
	const char* p_in = in_str;

	if (in_str == NULL || out_str == NULL || len <= 0) {
		return INVALID_PARAM;
	}

	while(len-- > 0) {
		if (move_step == PACK_UNIT_SIZE) {
			move_step = 0;
			mask = p_out;
			p_out++;
		}
		if (*p_in) {
			*p_out = *p_in;
			*mask |= (1 << move_step);
			p_out++;
		}
		p_in++;
		move_step++;
	}
	return p_out - out_str;
}

int
sge_unpack(const char* in_str, int len, char* out_str) {
	int move_step = 0;
	char mask = *in_str;
	const char* p_in = in_str + 1;
	char* p_out = out_str;
	unsigned char c = 0;

	if (in_str == NULL || out_str == NULL || len == 0) {
		return INVALID_PARAM;
	}

	len--;
	while(len > 0) {
		c = mask & (0x01 << move_step);
		if (c) {
			*p_out = *p_in;
			len--;
			p_in++;
		} else {
			*p_out = 0x00;
		}
		move_step++;
		p_out++;
		if (move_step == PACK_UNIT_SIZE) {
			move_step = 0;
			mask = *p_in;
			len--;
			p_in++;
		}
	}

	return p_out - out_str;
}

void
sge_destroy(int clean) {
	sge_block* block;
	sge_list* pb, *pb_next;

	for (pb = protocol.block_head.next; !LIST_EMPTY(&protocol.block_head); ) {
		pb_next = pb->next;
		block = LIST_DATA(pb, sge_block, head);
		sge_destroy_block(block);
		pb = pb_next;
	}

	if (clean) {
		memset(protocol.err, 0, sizeof(protocol.err));
	}

	sge_table_destroy(protocol.ht_name);
	sge_table_destroy(protocol.ht_idx);
	protocol.init = 0;
}

void
sge_print() {
	sge_block* block;
	sge_field* field;
	sge_list* pb, *pf;

	LIST_FOREACH(pb, &(protocol.block_head)) {
		block = LIST_DATA(pb, sge_block, head);
		printf("block name: %s, field size: %d\n", block->name, block->size);
		LIST_FOREACH(pf, &(block->field_head)) {
			field = LIST_DATA(pf, sge_field, head);
			field->type->ops->print(field, 1);
		}
	}
}

static const char* sge_error_str[] = {
	"Success",
	"Unknown error",
	"Invalid Param",
	"No such file or directory",
	"NOT SCHEME"
};

const char*
sge_error(int code) {
	code = -code;
	if (code < MIN_ERROR_CODE || code > MAX_ERROR_CODE) {
		return sge_error_str[1];
	}
	if (code == 1) {
		return protocol.err;
	}
	return sge_error_str[code];
}
