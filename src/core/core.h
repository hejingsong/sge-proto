#ifndef SGE_PROTO_CORE_H_
#define SGE_PROTO_CORE_H_

#include "proto.h"

#define SGE_DICT_SLOT_SIZE 64
#define SGE_INTEGER_SIZE 8
#define UNCOMPRESS_OUTPUT_LEN 10240

#define SGE_PROTO_ERROR_MSG(p, c, m)        \
  {                                         \
    (p)->err.code = (c);                    \
    memcpy((p)->err.msg, m, strlen(m) + 1); \
  }

#define SGE_PROTO_ERROR(p, c) SGE_PROTO_ERROR_MSG(p, c, ERROR_MSG[(c)])

#define SGE_PROTO_ERROR_ARG(p, c, err, ...)      \
  {                                              \
    size_t l = 0;                                \
    char buf[1024];                              \
    l = snprintf(buf, 1024, err, ##__VA_ARGS__); \
    buf[l] = '\0';                               \
    SGE_PROTO_ERROR_MSG((p), (c), buf);          \
  }

#define HAS_ERROR(e) ((e)->code != 0)

#define sge_unused(p) (void)(p)
#define sge_malloc malloc
#define sge_calloc(s) calloc(1, s)
#define sge_free(p) free((void *)(p))

struct list {
  struct list *next;
  struct list *prev;
};

typedef struct sge_dict sge_dict;
typedef struct sge_value sge_value;
typedef struct sge_string sge_string;
typedef struct sge_field sge_field;
typedef struct sge_block sge_block;
typedef struct sge_proto sge_proto;
typedef struct sge_parser sge_parser;
typedef struct sge_encoder sge_encoder;
typedef struct sge_decoder sge_decoder;
typedef struct sge_dict_node sge_dict_node;
typedef struct sge_dict_iter sge_dict_iter;
typedef enum sge_field_type sge_field_type;

#define SGE_FIELD_TYPE(t) sge_value_type(t)
#define SGE_IS_LIST(t) sge_is_list(t)

typedef enum {
  FLAG_REQUIRED = 1 << 0,
  FLAG_OPTIONAL = 1 << 1,
  FLAG_UNKNOWN = 1 << 15
} sge_field_flag;

struct sge_dict_node {
  struct list entry;
  const char *k;
  size_t kl;
  void *data;
};

struct sge_dict_iter {
  struct sge_dict *d;
  int slot;
  struct list *node;
};

struct sge_dict {
  struct list slots[SGE_DICT_SLOT_SIZE];
};

struct sge_field {
  unsigned int id : 32;
  unsigned int t : 16;  // type
  unsigned int flags : 16;
  const char *t_name;  // custom type name
  const char *name;
};

struct sge_block {
  unsigned int id : 24;
  unsigned int nf : 8;  // number of field
  sge_field *fields;
  char *name;
};

struct sge_parser {
  const char *content;
  const char *cursor;
  size_t size;
  size_t lineno;
};

struct sge_proto {
  sge_dict blocks;
  sge_block **block_arr;
  sge_integer max_bid;
  sge_parser parser;
  struct {
    int code;
    char msg[1024];
  } err;
};

#define sge_container_of(ptr, type, member) \
  (type *)((void *)(ptr) - (void *)(&(((type *)0)->member)))

#define SGE_LIST_INIT(list) \
  do {                      \
    (list)->next = (list);  \
    (list)->prev = (list);  \
  } while (0)

#define SGE_LIST_APPEND(list, node) \
  do {                              \
    (node)->next = (list);          \
    (node)->prev = (list)->prev;    \
    (list)->prev->next = (node);    \
    (list)->prev = (node);          \
  } while (0)

#define SGE_LIST_REMOVE(node)          \
  do {                                 \
    (node)->prev->next = (node)->next; \
    (node)->next->prev = (node)->prev; \
  } while (0)

#define SGE_LIST_EMPTY(list) ((list)->next == (list))

#define SGE_LIST_FOREACH(iter, list) \
  for ((iter) = (list)->next; (iter) != (list); (iter) = (iter)->next)

#define SGE_LIST_FOREACH_SAFE(iter, next, list)                        \
  for ((iter) = (list)->next, (next) = (iter)->next; (iter) != (list); \
       (iter) = (next), (next) = (next)->next)

void sge_init_dict(sge_dict *d);
void sge_insert_dict(sge_dict *d, const char *k, size_t kl, void *data);
void *sge_get_dict(sge_dict *d, const char *k, size_t kl);
void sge_del_dict(sge_dict *d, const char *k, size_t kl);
void sge_free_dict(sge_dict *d);

void sge_init_dict_iter(sge_dict_iter *iter, sge_dict *d);
void *sge_dict_iter_next(sge_dict_iter *iter);

#endif
