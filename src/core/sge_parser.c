#include <stdio.h>
#include <string.h>
#include "sge_parser.h"


#define COMMENT_CHAR '#'
#define UTF8_BOM_STR "\xEF\xBB\xBF"
#define UTF8_BOM_SIZE 3
#define LEFT_BODY_CHAR '{'
#define RIGHT_BODY_CHAR '}'
#define FIELD_DELIMITER ':'
#define FIELD_TERMINATOR ';'
#define PACK_UNIT_SIZE 8

#define VALID_NUMBER(c) (c >= 48 && c <= 57)
#define VALID_CHAR(c) ((c >= 65 && c <= 90) || (c >= 97 && c <= 122) || (c == 95))

static int
verify_char(char c) {
	if (VALID_CHAR(c) || VALID_NUMBER(c)) {
		return SGE_OK;
	}
	return SGE_ERR;
}

static void
filter_utf8_bom(sge_text* const text) {
	if ((NULL == text) || (NULL == text->data) || (NULL == text->cursor)) {
		return;
	}

	if (strncmp(text->cursor, UTF8_BOM_STR, UTF8_BOM_SIZE) == 0) {
		text->cursor += UTF8_BOM_SIZE;
	}
}

static int
empty_buffer(sge_text* text) {
	return (text->cursor - text->data) >= text->len;
}

static int
filter_blank_char(sge_text* text) {
	char c = *text->cursor;

	if (c > 32) {
		return SGE_ERR;
	}
	text->cursor++;
	if (c == 10) {
		text->lineno++;
	}
	return SGE_OK;
}

static void
trim_right(sge_text* text) {
	if (empty_buffer(text)) {
		return;
	}

	while(filter_blank_char(text) == SGE_OK);
}

static void
filter_comment_line(sge_text* text) {
	char c;
	char* p;

	RETRY:
	trim_right(text);
	c = *text->cursor;
	if (c == COMMENT_CHAR) {
		p = strchr(text->cursor, '\n');
		if (p == NULL) {
			return;
		}
		text->cursor = p + 1;
		text->lineno++;
		goto RETRY;
	}
}

static int
parse_field_delimiter(sge_proto* proto) {
	sge_text* text = &proto->text;
	char c = *text->cursor;
	if (c != FIELD_DELIMITER) {
		SET_ERROR(proto, "parse field fail at line %ld\n", text->lineno);
		return SGE_ERR;
	}
	text->cursor++;
	return SGE_OK;
}

static uint32_t
power(uint32_t x, uint32_t y) {
	uint32_t num = x;
	if (y == 0) {
		return 1;
	}
	while (--y) {
		x *= num;
	}
	return x;
}

static uint32_t
str2u32(const char* p, uint32_t len) {
	uint32_t num = 0;

	if (0 == len) {
		return num;
	}

	while (*p && len--) {
		num += ((*p) - '0') * power(10, len);
		++p;
	}
	return num;
}

static sge_block*
alloc_block(const char* block_name, size_t name_len, const char* proto_idx, size_t idx_len) {
	uint32_t idx = str2u32(proto_idx, idx_len);
	return sge_alloc_block(block_name, name_len, idx);
}

static sge_block*
add_block(sge_proto* proto, sge_block* block) {
	LIST_ADD_TAIL(&(proto->block_head), &(block->head));
	sge_table_insert(proto->ht_name, block->name, strlen(block->name), block);
	sge_table_insert(proto->ht_idx, (void*)&block->idx, 0, block);
    return SGE_OK;
}

static int
parse_name(sge_proto* proto, char* err_info, const char** name) {
	char c;
	const char* p;
	sge_text* text = &proto->text;

	c = *text->cursor;
	p = text->cursor;
	if (VALID_NUMBER(c)) {
		SET_ERROR(proto, "invalid %s name at line %ld.\n", err_info, text->lineno);
		return SGE_ERR;
	}

	while(c) {
		if (SGE_ERR == verify_char(c)) {
			break;
		}
		text->cursor++;
		c = *text->cursor;
	}
	if (p == text->cursor) {
		SET_ERROR(proto, "invalid %s name at line %ld.\n", err_info, text->lineno);
		return SGE_ERR;
	}
	*name = p;
	return text->cursor - p;
}

static int
parse_field_end(sge_proto* proto) {
	sge_text* text = &proto->text;
	char c = *text->cursor;
	if (c != FIELD_TERMINATOR) {
		SET_ERROR(proto, "can't found field terminator at line %ld\n", text->lineno);
		return SGE_ERR;
	}
	text->cursor++;
	return SGE_OK;
}

static int
parse_field_type(sge_proto* proto, const char** field_type) {
	int len = 0;
	sge_text* text = &proto->text;
	len = parse_name(proto, "field type", field_type);
	if (len == SGE_ERR) {
		return SGE_ERR;
	}
	if (0 == strncmp(text->cursor, "[]", 2)) {
		text->cursor += 2;
		len += 2;
	}
	return len;
}

static sge_field*
parse_protocol_field(sge_proto* proto) {
	const char* field_name = NULL, *field_type = NULL;
	int field_name_len = 0, field_type_len = 0;
	sge_text* text = &proto->text;
	sge_field* field;

	field_name_len = parse_name(proto, "field", &field_name);
	if (field_name_len == SGE_ERR) {
		return NULL;
	}
	filter_comment_line(text);
	parse_field_delimiter(proto);
	filter_comment_line(text);
	field_type_len = parse_field_type(proto, &field_type);
	if (field_type_len == SGE_ERR) {
		return NULL;
	}
	filter_comment_line(text);
	if (SGE_ERR == parse_field_end(proto)) {
		return NULL;
	}

	field = sge_add_field(field_name, field_name_len, field_type, field_type_len);
	if (NULL == field) {
		SET_ERROR(proto, "can't found field type: %.*s at line: %ld", field_type_len, field_type, text->lineno);
	}
	return field;
}


static int
parse_protocol_body(sge_proto* proto, sge_list* field_head) {
	char c;
	int field_size = 0;
	sge_field* field;
	sge_text* text = &proto->text;

	c = *text->cursor;
	if (c != LEFT_BODY_CHAR) {
		SET_ERROR(proto, "can't found '{' at line %ld\n", text->lineno);
		return SGE_ERR;
	}

	text->cursor++;
	c = *text->cursor;
	while(c) {
		filter_comment_line(text);
		c = *text->cursor;
		if (c == RIGHT_BODY_CHAR) {
			break;
		}
		field = parse_protocol_field(proto);
		if (field == NULL) {
			goto RET;
		}
		
		LIST_ADD_TAIL(field_head, &(field->head));
		field_size++;
		c = *text->cursor;
	}

	if (c != RIGHT_BODY_CHAR) {
		SET_ERROR(proto, "can't found '}' at line %ld\n", text->lineno);
		return SGE_ERR;
	}
	if (field_size == 0) {
		return field_size;
	}
	text->cursor++;
	return field_size;
RET:
	return SGE_ERR;
}

static int
parse_protocol_idx(sge_proto* proto, const char** p_idx) {
	char c;
	const char* p;
	sge_text* text = &proto->text;

	c = *text->cursor;
	p = text->cursor;
	while(c) {
		if (!VALID_NUMBER(c)) {
			break;
		}
		text->cursor++;
		c = *text->cursor;
	}
	if (p == text->cursor) {
		SET_ERROR(proto, "invalid protocol idx at line %ld.\n", text->lineno);
		return SGE_ERR;
	}
	*p_idx = p;
	return text->cursor - p;
}

static int
parse_protocol(sge_proto* proto) {
	sge_block *block = NULL;
	const char* proto_name = NULL, *proto_idx_str = NULL;
	int proto_name_len = 0, proto_idx_len = 0, field_size = 0;
	sge_text* text = &proto->text;

	filter_comment_line(text);
	if (empty_buffer(text)) {
		return SGE_OK;
	}
	proto_name_len = parse_name(proto, "protocol", &proto_name);
	if (proto_name_len == SGE_ERR) {
		return SGE_ERR;
	}

	filter_comment_line(text);
	proto_idx_len = parse_protocol_idx(proto, &proto_idx_str);
	if (proto_idx_len == SGE_ERR) {
		return SGE_ERR;
	}

	block = alloc_block(proto_name, proto_name_len, proto_idx_str, proto_idx_len);

	filter_comment_line(text);
	field_size = parse_protocol_body(proto, &block->field_head);
	if (field_size <= 0) {
		if (field_size == 0) {
			SET_ERROR(proto, "protocol: %.*s is empty at line %ld\n", proto_name_len, proto_name, text->lineno);
		}
		goto ERR;
	}
	block->size = field_size;
	add_block(proto, block);
	return parse_protocol(proto);
ERR:
	sge_destroy_block(block);
	return SGE_ERR;
}

// export
int
sge_parse_protocol(sge_proto* proto) {
	filter_utf8_bom(&(proto->text));
	return parse_protocol(proto);
}
