#ifndef SGE_PROTO_H_
#define SGE_PROTO_H_

#include <stdlib.h>

#define SGE_PROTO_MAJOR_VERSION 0
#define SGE_PROTO_MINOR_VERSION 1
#define SGE_PROTO_VERSION \
  ((1 << (SGE_PROTO_MAJOR_VERSION + 4)) | SGE_PROTO_MINOR_VERSION)
#define SGE_PROTO_VERSION_STR "SGE_PROTO_VERSION"

#define SGE_OK 0
#define SGE_ERR -1

#define sge_integer long long
#define sge_number double

#define sge_unused(p) (void)(p)
#define sge_value_type(t) ((t) & (~0x4000))
#define sge_is_list(t) ((t)&FIELD_TYPE_LIST)

#define sge_value_integer(v, vv) \
  {                              \
    (v)->t = FIELD_TYPE_INTEGER; \
    (v)->v.i = (vv);               \
  }
#define sge_value_number(v, vv) \
  {                             \
    (v)->t = FIELD_TYPE_NUMBER; \
    (v)->v.n = (vv);              \
  }
#define sge_value_string(v, vv) \
  {                             \
    (v)->t = FIELD_TYPE_STRING; \
    (v)->v.s.s = (vv);            \
    (v)->v.s.l = strlen((vv));    \
  }
#define sge_value_string_with_len(v, vv, vl) \
  {                                          \
    (v)->t = FIELD_TYPE_STRING;              \
    (v)->v.s.s = (vv);                         \
    (v)->v.s.l = (vl);                         \
  }
#define sge_value_any(v, vv)    \
  {                             \
    (v)->t = FIELD_TYPE_CUSTOM; \
    (v)->v.a = (vv);              \
  }
#define sge_value_list(v, vv) \
  {                           \
    (v)->t = FIELD_TYPE_LIST; \
    (v)->v.a = (vv);            \
  }
#define sge_value_nil(v) \
  { (v)->t = FIELD_TYPE_UNKNOWN; }

struct sge_proto;

// error code
enum {
  SGE_ERR_FILE_NOT_FOUND = 1,
  SGE_ERR_MEMORY_NOT_ENOUGH,
  SGE_ERR_PARSER_ERROR,
  SGE_ERR_BLOCK_NAME_NOT_FOUND,
  SGE_ERR_ENCODE_ERROR,
  SGE_ERR_DECODE_ERROR,
  SGE_ERR_NOT_FOUND_PROTO
};

enum sge_field_type {
  FIELD_TYPE_INTEGER = 1 << 0,
  FIELD_TYPE_NUMBER = 1 << 1,
  FIELD_TYPE_STRING = 1 << 2,
  FIELD_TYPE_CUSTOM = 1 << 3,
  FIELD_TYPE_LIST = 1 << 14,
  FIELD_TYPE_UNKNOWN = 1 << 15
};

enum sge_key_type { KT_LIST_INDEX, KT_STRING };

struct sge_string {
  const char *s;
  size_t l;
};

struct sge_value {
  unsigned int t;  // field type
  union {
    sge_integer i;
    sge_number n;
    struct sge_string s;
    void *a;  // any
  } v;
};

struct sge_key {
  unsigned char t;
  size_t idx;
  struct sge_string name;
  struct sge_string t_name;
};

typedef int (*sge_fn_get)(const void *, const struct sge_key *,
                          struct sge_value *);
typedef void *(*sge_fn_set)(void *, const struct sge_key *,
                            const struct sge_value *);

struct sge_proto *sge_parse(const char *filename);
struct sge_proto *sge_parse_content(const char *content, size_t len);
int sge_free_proto(struct sge_proto *p);

// debug
void sge_print_proto(struct sge_proto *p);
int sge_proto_error(struct sge_proto *p, const char **errp);

struct sge_string *sge_encode(struct sge_proto *p, const char *name,
                              const void *ud, sge_fn_get fn_get);
int sge_decode(struct sge_proto *p, const char *s, size_t len, void *ud,
               sge_fn_set fn_set);

void sge_free_string(struct sge_string *s);

#endif
