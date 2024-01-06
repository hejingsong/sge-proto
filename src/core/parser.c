#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "core.h"

#define NEWLINE_CHAR '\n'
#define COMMENT_CHAR '#'
#define UTF8_BOM_STR "\xEF\xBB\xBF"
#define LEFT_BODY_CHAR '{'
#define RIGHT_BODY_CHAR '}'
#define FIELD_DELIMITER ':'
#define FIELD_TERMINATOR ';'
#define UTF8_BOM_SIZE 3

#define SGE_EOF(p) (*(p)->cursor == '\0')

#define VALID_NUMBER(c) ((c) >= 48 && (c) <= 57)
#define VALID_CHAR(c) \
  (((c) >= 65 && (c) <= 90) || ((c) >= 97 && (c) <= 122) || ((c) == 95))
#define VERIFY_CHAR(c) (VALID_CHAR((c)) || VALID_NUMBER((c)))

#define APPEND_FIELD(h, f)                                                 \
  {                                                                        \
    struct list *__l = (struct list *)((char *)(f) - sizeof(struct list)); \
    SGE_LIST_APPEND((h), __l);                                             \
  }

#define DESTROY_FIELD(p, full)                                   \
  {                                                              \
    struct sge_field *__f =                                      \
        (struct sge_field *)((char *)(p) + sizeof(struct list)); \
    destroy_field(__f, (full));                                  \
  }

const char *ERROR_MSG[] = {
    "SUCCESS",      "FILE NOT FOUND",       "MEMORY NOT ENOUGH",
    "PARSER ERROR", "BLOCK NAME NOT FOUND", "ENCODE ERROR",
    "DECODE ERROR", "NOT FOUND ANY PROTOCOL DEFINED"};

static const char *FIELD_FLAGS[] = {"required", "optional", NULL};

static const char *FIELD_TYPES[] = {"integer", "number", "string", NULL};

static sge_proto *alloc_proto(void) {
  sge_proto *p = NULL;

  p = (sge_proto *)sge_malloc(sizeof(*p));
  if (NULL == p) {
    return NULL;
  }

  p->err.code = 0;
  memset(p->err.msg, 0, 1024);
  p->parser.content = p->parser.cursor = NULL;
  p->parser.lineno = 1;
  p->parser.size = 0;
  sge_init_dict(&p->blocks);

  return p;
}

static sge_field *alloc_field(void) {
  struct list *f = NULL;

  f = (struct list *)sge_malloc(sizeof(struct list) + sizeof(struct sge_field));
  if (NULL == f) {
    return NULL;
  }

  SGE_LIST_INIT(f);

  return (sge_field *)(f + 1);
}

static void destroy_field(sge_field *f, int full) {
  void *p = NULL;
  if (NULL == f) {
    return;
  }

  if (full) {
    sge_free(f->name);
    if (f->t_name) {
      sge_free(f->t_name);
    }
  }

  p = (struct list *)f - 1;
  sge_free(p);
}

static void move_cursor(sge_parser *parser) {
  char c = 0;

  parser->cursor++;
  c = *parser->cursor;
  if (c == NEWLINE_CHAR) {
    parser->lineno++;
    parser->cursor++;
  }
}

static void trim(sge_parser *parser) {
  char c = 0;

  while (!SGE_EOF(parser)) {
    c = *parser->cursor;
    if (c > 32) {
      break;
    }

    if (c == NEWLINE_CHAR) {
      parser->lineno++;
    }
    parser->cursor++;
  }
}

static void filter_utf8_bom(sge_parser *parser) {
  if (strncmp(parser->cursor, UTF8_BOM_STR, UTF8_BOM_SIZE) == 0) {
    parser->cursor += UTF8_BOM_SIZE;
  }
}

static void filter_comment(sge_parser *parser) {
  char c = 0;
  char *p = NULL;

  while (!SGE_EOF(parser)) {
    trim(parser);
    c = *parser->cursor;
    if (c == COMMENT_CHAR) {
      p = strchr(parser->cursor, NEWLINE_CHAR);
      if (p == NULL) {
        // the last line
        parser->cursor = parser->content + parser->size + 1;
        continue;
      }
      parser->cursor = p + 1;
      parser->lineno++;
    } else {
      break;
    }
  }
}

static size_t parse_string(sge_parser *parser, const char **strp) {
  char c = 0;
  const char *p = NULL;
  size_t l = 0;

  filter_comment(parser);
  if (SGE_EOF(parser)) {
    *strp = NULL;
    return l;
  }

  p = parser->cursor;
  c = *parser->cursor;
  while (c && VERIFY_CHAR(c)) {
    move_cursor(parser);
    c = *parser->cursor;
  }

  l = parser->cursor - p;

  *strp = p;
  return l;
}

static int field_flag(const char *flag, const size_t fl) {
  int i = 0;
  const char *p = NULL;

  if (0 == fl) {
    return FLAG_OPTIONAL;
  }

  while (FIELD_FLAGS[i]) {
    p = FIELD_FLAGS[i];
    if (0 == strncmp(p, flag, fl)) {
      return 1 << i;
    }
    ++i;
  }

  return FLAG_UNKNOWN;
}

static int field_type(const char *type, const size_t tl) {
  int i = 0;
  const char *p = NULL;

  if (0 == tl) {
    return FIELD_TYPE_UNKNOWN;
  }

  while (FIELD_TYPES[i]) {
    p = FIELD_TYPES[i];
    if (0 == strncmp(p, type, tl)) {
      return 1 << i;
    }
    i++;
  }

  return FIELD_TYPE_UNKNOWN;
}

static size_t parse_block_name(sge_proto *p, const char **namep) {
  char c = 0;
  size_t len = 0;
  const char *name = NULL;
  sge_parser *parser = &p->parser;

  filter_comment(parser);

  if (SGE_EOF(parser)) {
    goto out;
  }

  c = *parser->cursor;
  if (VALID_NUMBER(c)) {
    SGE_PROTO_ERROR_ARG(p, SGE_ERR_PARSER_ERROR,
                        "invalid block name at line: %lu", parser->lineno);
    goto out;
  }

  len = parse_string(parser, &name);
  if (0 == len) {
    SGE_PROTO_ERROR_ARG(p, SGE_ERR_PARSER_ERROR,
                        "can't found block name at line: %lu", parser->lineno);
    goto out;
  }

out:
  *namep = name;
  return len;
}

static void parse_block_id(sge_proto *p, int *idp) {
  char c = 0;
  int id = 0;
  size_t len = 0;
  const char *start = NULL;
  char s[24] = {0};
  sge_parser *parser = &p->parser;

  filter_comment(parser);

  if (SGE_EOF(parser)) {
    SGE_PROTO_ERROR_ARG(p, SGE_ERR_PARSER_ERROR,
                        "incomplete block at line: %lu", parser->lineno);
    goto out;
  }

  c = *parser->cursor;
  if (!VALID_NUMBER(c)) {
    SGE_PROTO_ERROR_ARG(p, SGE_ERR_PARSER_ERROR,
                        "invalid block id at line: %lu", parser->lineno);
    goto out;
  }

  start = parser->cursor;
  while (c && VALID_NUMBER(c)) {
    move_cursor(parser);
    c = *parser->cursor;
  }

  if (start == parser->cursor) {
    SGE_PROTO_ERROR_ARG(p, SGE_ERR_PARSER_ERROR,
                        "can't found block id at line: %lu", parser->lineno);
    goto out;
  }

  len = parser->cursor - start;
  memcpy(s, start, len);
  s[len] = '\0';
  id = atoi(s);

  if (id <= 0) {
    SGE_PROTO_ERROR_ARG(p, SGE_ERR_PARSER_ERROR,
                        "block id must be greater than 0 at line: %lu",
                        parser->lineno);
    goto out;
  }

out:
  *idp = id;
}

static sge_field *parse_field(sge_proto *p) {
  int type = 0, flags = 0;
  char c = 0, is_arr = 0;
  sge_field *field = NULL;
  sge_parser *parser = &p->parser;
  size_t nl = 0, fl = 0, tl = 0;
  const char *name = NULL, *f = NULL, *t = NULL;
  char *str = NULL;

  // parse field name
  nl = parse_string(parser, &name);
  if (0 == nl) {
    SGE_PROTO_ERROR_ARG(p, SGE_ERR_PARSER_ERROR,
                        "invalid field name at line: %lu", parser->lineno);
    return NULL;
  }

  // parse field type
  tl = parse_string(parser, &t);
  if (0 == tl) {
    SGE_PROTO_ERROR_ARG(p, SGE_ERR_PARSER_ERROR,
                        "invalid field type at line: %lu", parser->lineno);
    return NULL;
  }
  if (0 == strncmp(parser->cursor, "[]", 2)) {
    is_arr = 1;
    parser->cursor += 2;
  }

  // parse field flag
  // fl == 0 mean optional
  fl = parse_string(parser, &f);
  flags = field_flag(f, fl);
  if (flags == FLAG_UNKNOWN) {
    SGE_PROTO_ERROR_ARG(p, SGE_ERR_PARSER_ERROR,
                        "unknown field flag at line: %lu", parser->lineno);
    return NULL;
  }

  // parse field end
  filter_comment(parser);
  if (SGE_EOF(parser)) {
    SGE_PROTO_ERROR_ARG(p, SGE_ERR_PARSER_ERROR, "invalid syntax at line: %lu",
                        parser->lineno);
    return NULL;
  }
  c = *parser->cursor;
  if (c != FIELD_TERMINATOR) {
    SGE_PROTO_ERROR_ARG(p, SGE_ERR_PARSER_ERROR, "invalid syntax at line: %lu",
                        parser->lineno);
    return NULL;
  }
  move_cursor(parser);

  type = field_type(t, tl);
  if (is_arr) {
    type |= FIELD_TYPE_LIST;
  }

  str = (char *)sge_malloc(nl + 1);
  if (NULL == str) {
    return NULL;
  }
  memcpy(str, name, nl);
  str[nl] = '\0';
  name = str;

  if (type & FIELD_TYPE_UNKNOWN) {
    str = (char *)sge_malloc(tl + 1);
    if (NULL == str) {
      sge_free(name);
      return NULL;
    }
    memcpy(str, t, tl);
    str[tl] = '\0';
    t = str;
  } else {
    t = NULL;
  }

  field = alloc_field();
  if (NULL == field) {
    sge_free(name);
    if (t) {
      sge_free(t);
    }
    return NULL;
  }

  field->flags = flags;
  field->id = 0;
  field->name = name;
  field->t_name = t;
  field->t = type;

  return field;
}

static void parse_block_body(sge_proto *p, int *nfp, sge_field **fsp) {
  int nf = 0;
  char c = 0, fin = 0;
  sge_field *f = NULL, *farr = NULL;
  struct list fl, *iter = NULL, *next = NULL;
  sge_parser *parser = &p->parser;

  filter_comment(parser);
  if (SGE_EOF(parser)) {
    SGE_PROTO_ERROR_ARG(p, SGE_ERR_PARSER_ERROR,
                        "undefined block body at line: %lu", parser->lineno);
    return;
  }

  c = *parser->cursor;
  if (c != LEFT_BODY_CHAR) {
    SGE_PROTO_ERROR_ARG(p, SGE_ERR_PARSER_ERROR, "invalid syntax at line: %lu",
                        parser->lineno);
    return;
  }

  SGE_LIST_INIT(&fl);
  move_cursor(parser);
  while (!SGE_EOF(parser)) {
    filter_comment(parser);
    c = *parser->cursor;
    if (c == RIGHT_BODY_CHAR) {
      fin = 1;
      move_cursor(parser);
      break;
    }

    f = parse_field(p);
    if (NULL == f) {
      break;
    }

    f->id = ++nf;
    APPEND_FIELD(&fl, f);
  }

  if (fin == 0) {
    SGE_LIST_FOREACH_SAFE(iter, next, &fl) {
      SGE_LIST_REMOVE(iter);
      DESTROY_FIELD(iter, 1);
    }

    SGE_PROTO_ERROR_ARG(p, SGE_ERR_PARSER_ERROR,
                        "not found block terminator syntax at line: %lu",
                        parser->lineno);
    *nfp = 0;
    *fsp = NULL;
    return;
  }

  *nfp = nf;
  if (nf == 0) {
    *fsp = NULL;
  } else {
    *fsp = farr = (sge_field *)sge_malloc(sizeof(sge_field) * nf);

    SGE_LIST_FOREACH_SAFE(iter, next, &fl) {
      f = (struct sge_field *)((char *)iter + sizeof(struct list));
      memcpy(farr, f, sizeof(sge_field));
      farr += 1;

      SGE_LIST_REMOVE(iter);
      DESTROY_FIELD(iter, 0);
    }
  }
}

static sge_block *alloc_block(int id, const char *name, size_t name_len, int nf,
                              sge_field *fs) {
  sge_block *b = NULL;
  char *buf;

  buf = (char *)sge_malloc(name_len + 1);
  if (NULL == buf) {
    return NULL;
  }

  memcpy(buf, name, name_len);
  buf[name_len] = '\0';

  b = (sge_block *)sge_malloc(sizeof(*b));
  if (NULL == b) {
    goto err;
  }

  b->name = buf;
  b->fields = fs;
  b->nf = nf;
  b->id = id;

out:
  return b;

err:
  sge_free(buf);
  goto out;
}

static void destroy_block(sge_block *b) {
  int i = 0;

  if (NULL == b) {
    return;
  }

  for (i = 0; i < b->nf; ++i) {
    sge_free(b->fields[i].name);
    if (b->fields[i].t_name) {
      sge_free(b->fields[i].t_name);
    }
  }

  sge_free(b->fields);
  sge_free(b->name);
  sge_free(b);
}

static sge_block *parse_one_block(sge_proto *p) {
  size_t name_len = 0;
  const char *name = NULL;
  int block_id = 0;
  int nf = 0;
  sge_field *fields = NULL;
  sge_block *block = NULL;

  name_len = parse_block_name(p, &name);
  if (HAS_ERROR(&p->err)) {
    return NULL;
  }

  parse_block_id(p, &block_id);
  if (HAS_ERROR(&p->err)) {
    return NULL;
  }

  parse_block_body(p, &nf, &fields);
  if (HAS_ERROR(&p->err)) {
    return NULL;
  }

  block = alloc_block(block_id, name, name_len, nf, fields);
  if (NULL == block) {
    SGE_PROTO_ERROR(p, SGE_ERR_MEMORY_NOT_ENOUGH);
    return NULL;
  }

  return block;
}

static void do_parse(sge_proto *p) {
  int i = 0;
  sge_integer max_bid = 0;
  void *data = NULL;
  sge_block *block = NULL, *tb = NULL;
  sge_field *f = NULL;
  sge_dict_iter iter;

  filter_utf8_bom(&p->parser);

  while (!SGE_EOF(&p->parser) && !HAS_ERROR(&p->err)) {
    filter_comment(&p->parser);
    if (SGE_EOF(&p->parser))
      break;

    block = parse_one_block(p);
    if (block) {
      if (block->id > max_bid) {
        max_bid = block->id;
      }
      sge_insert_dict(&p->blocks, block->name, strlen(block->name), block);
    } else {
      goto err;
    }
  }

  p->max_bid = max_bid;
  p->block_arr = (sge_block **)sge_calloc(sizeof(sge_block *) * max_bid);
  sge_init_dict_iter(&iter, &p->blocks);
  while ((data = sge_dict_iter_next(&iter))) {
    block = (sge_block *)data;

    p->block_arr[block->id - 1] = block;
    for (i = 0; i < block->nf; ++i) {
      f = &block->fields[i];
      if (f->t & FIELD_TYPE_UNKNOWN) {
        tb =
            (sge_block *)sge_get_dict(&p->blocks, f->t_name, strlen(f->t_name));
        if (NULL == tb) {
          SGE_PROTO_ERROR_ARG(p, SGE_ERR_PARSER_ERROR,
                              "not found custom type(%s) in block(%s:%s)",
                              f->t_name, block->name, f->name);
          goto err;
        } else {
          f->t &= ~FIELD_TYPE_UNKNOWN;
          f->t |= FIELD_TYPE_CUSTOM;
        }
      }
    }
  }
  return;

err:
  if (SGE_DICT_EMPTY(&p->blocks)) {
    SGE_PROTO_ERROR(p, SGE_ERR_NOT_FOUND_PROTO);
    return;
  }

  sge_init_dict_iter(&iter, &p->blocks);
  while ((data = sge_dict_iter_next(&iter))) {
    block = (sge_block *)data;
    sge_del_dict(&p->blocks, block->name, strlen(block->name));
    destroy_block(block);
  }
}

sge_proto *sge_parse(const char *filename) {
  int ret = 0;
  FILE *fp = NULL;
  char *buf = NULL;
  sge_proto *p = NULL;
  struct stat s;

  p = alloc_proto();
  if (NULL == p) {
    return NULL;
  }

  ret = stat(filename, &s);
  if (ret < 0) {
    SGE_PROTO_ERROR(p, SGE_ERR_FILE_NOT_FOUND);
    return p;
  }

  p->parser.size = s.st_size;

  buf = (char *)sge_malloc(s.st_size + 1);
  if (NULL == buf) {
    ret = SGE_ERR;
    SGE_PROTO_ERROR(p, SGE_ERR_MEMORY_NOT_ENOUGH);
    goto out;
  }

  fp = fopen(filename, "r");
  fread(buf, p->parser.size, 1, fp);
  fclose(fp);
  buf[p->parser.size] = '\0';

  p->parser.content = p->parser.cursor = buf;

  do_parse(p);
out:
  sge_free(p->parser.content);
  p->parser.content = NULL;
  return p;
}

sge_proto *sge_parse_content(const char *content, size_t len) {
  sge_proto *p = NULL;

  if (NULL == content || len == 0) {
    return NULL;
  }

  p = alloc_proto();
  if (NULL == p) {
    return NULL;
  }

  p->parser.size = len;
  p->parser.cursor = p->parser.content = content;

  do_parse(p);
  p->parser.cursor = p->parser.content = NULL;

  return p;
}

int sge_free_proto(sge_proto *p) {
  sge_dict_iter iter;
  void *data = NULL;
  sge_block *bp = NULL;

  if (NULL == p) {
    return SGE_ERR;
  }

  sge_init_dict_iter(&iter, &p->blocks);
  while ((data = sge_dict_iter_next(&iter))) {
    bp = (sge_block *)data;
    destroy_block(bp);
  }

  sge_free(p);

  return SGE_OK;
}

void sge_print_proto(sge_proto *p) {
  int i = 0;
  void *data = NULL;
  sge_block *bp = NULL;
  sge_dict_iter iter;

  sge_init_dict_iter(&iter, &p->blocks);
  while ((data = sge_dict_iter_next(&iter))) {
    bp = (sge_block *)data;
    printf("%s %d\n", bp->name, bp->id);
    for (i = 0; i < bp->nf; ++i) {
      printf("\t%s %d %d\n", bp->fields[i].name, bp->fields[i].t,
             bp->fields[i].flags);
    }
  }
}

int sge_proto_error(sge_proto *p, const char **errp) {
  if (NULL == p) {
    return SGE_ERR;
  }

  *errp = p->err.msg;
  return p->err.code;
}
