#ifndef SGE_PROTO_H_
#define SGE_PROTO_H_

#include "sge_define.h"

int sge_parse(const char* text);
int sge_parse_file(const char* file);
int sge_encode(const char* name, const void *ud, char* buffer, field_get cb);
int sge_decode(const char* buffer, void* ud, field_set cb);
int sge_pack(const char* in_str, int len, char* out_str);
int sge_unpack(const char* in_str, int len, char* out_str);
void sge_destroy(int clean);
void sge_print();
const char* sge_error(int code);

#endif
