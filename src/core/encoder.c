#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core.h"

extern const char *ERROR_MSG[];

struct sge_result {
  sge_string s;
  size_t cap;
};

struct sge_encoder {
  sge_proto *p;
  sge_block *b;
  const void *ud;
  sge_fn_get get;
  struct sge_result *result;
};

struct sge_decoder {
  sge_proto *p;
  sge_block *b;
  struct sge_string s;
  size_t cursor;
  void *ud;
  sge_fn_set set;
};

static int do_encode(sge_encoder *encoder);
static int encode_normal(sge_encoder *encoder, sge_field *f);
static int encode_list(sge_encoder *encoder, sge_field *f);
static int encode_field(sge_encoder *encoder, sge_field *f);

static int do_decode(sge_decoder *decoder);

static void fill_key(struct sge_key *k, sge_field *f) {
  k->name.s = f->name;
  k->name.l = strlen(f->name);
  k->t_name.s = f->t_name;
  k->t_name.l = f->t_name ? strlen(f->t_name) : 0;
}

static int compress(sge_encoder *encoder) {
  int move_step = 0;
  size_t src_len = encoder->result->s.l - 3;
  const char *src = encoder->result->s.s + 3;
  char *dest = (char *)sge_malloc(encoder->result->s.l);
  char *mask = dest + 3;
  char *out = dest + 4;

  *mask = 0;
  while (src_len-- > 0) {
    if (move_step == 8) {
      move_step = 0;
      mask = out;
      *mask = 0;
      out++;
    }
    if (*src) {
      *out = *src;
      *mask |= (1 << move_step);
      out++;
    }
    src++;
    move_step++;
  }

  memcpy(dest, encoder->result->s.s, 3);
  sge_free(encoder->result->s.s);
  encoder->result->s.s = dest;
  encoder->result->s.l = out - dest;
  return SGE_OK;
}

static ssize_t uncompress(const char *str, size_t str_len, char *dest) {
  int move_step = 0;
  unsigned char c = 0;
  ssize_t len = str_len - 3;
  const char *src = str + 3;
  const char *p_in = src + 1;
  char *p_out = dest + 3;
  unsigned char mask = *src;

  len--;
  while (len > 0) {
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
    if (move_step == 8) {
      move_step = 0;
      mask = *p_in;
      len--;
      p_in++;
    }
  }

  memcpy(dest, str, 3);
  return p_out - dest;
}

static uint16_t crc16(struct sge_string *s) {
  static const int POLYNOMIAL = 0x1021;
  size_t i = 0, j = 0;
  int val = 0;
  uint16_t crc16 = 0;

  for (i = 0; i < s->l; i++) {
    val = s->s[i] << 8;
    for (j = 0; j < 8; j++) {
      if ((crc16 ^ val) & 0x8000) {
        crc16 = (crc16 << 1) ^ POLYNOMIAL;
      } else {
        crc16 <<= 1;
      }
      val <<= 1;
    }
  }

  return crc16;
}

static int set_crc16(sge_encoder *encoder) {
  uint16_t crc = 0;
  struct sge_string s;
  char *dest = (char *)(encoder->result->s.s + 1);

  s.s = encoder->result->s.s + 3;
  s.l = encoder->result->s.l - 3;
  crc = crc16(&s);

  memcpy(dest, &crc, 2);

  return SGE_OK;
}

static int check_crc(const char *str, size_t len) {
  uint16_t crc = 0;
  struct sge_string s;

  s.l = len - 3;
  s.s = str + 3;

  crc = crc16(&s);
  if (0 == memcmp(&crc, str + 1, 2)) {
    return SGE_OK;
  } else {
    return SGE_ERR;
  }
}

static void destroy_string(struct sge_result *r) {
  if (NULL == r) {
    return;
  }
  sge_free(r->s.s);
  sge_free(r);
}

static void append_string(sge_encoder *encoder, const char *s, size_t l) {
  char *buf = NULL;

  if (NULL == encoder->result) {
    encoder->result =
        (struct sge_result *)sge_malloc(sizeof(struct sge_result));
    encoder->result->s.s = (const char *)sge_malloc(64);
    encoder->result->cap = 64;
    encoder->result->s.l = 0;
  }

  buf = (char *)encoder->result->s.s;
  if (encoder->result->s.l + l > encoder->result->cap) {
    buf = (char *)realloc(buf, encoder->result->cap + l);
    encoder->result->cap += l;
  }

  memcpy(buf + encoder->result->s.l, s, l);
  encoder->result->s.l += l;
  encoder->result->s.s = buf;
}

static void encode_integer(sge_encoder *encoder, sge_integer i) {
  char c = 0;
  int step = 0, size = SGE_INTEGER_SIZE;
  unsigned sge_integer mask = 0xFF;

  for (step = 0; step < size; ++step) {
    c = (i >> (step * 8)) & mask;
    append_string(encoder, &c, 1);
  }
}

static void encode_number(sge_encoder *encoder, sge_number n) {
  // not supported yet
  sge_unused(encoder);
  sge_unused(n);
}

static void encode_string(sge_encoder *encoder, sge_string *s) {
  encode_integer(encoder, s->l);
  append_string(encoder, s->s, s->l);
}

static int encode_value(sge_encoder *encoder, sge_field *f, sge_value *v) {
  int ret = SGE_OK;
  const void *old_ud = NULL;
  sge_block *old_b = NULL;

  switch (v->t) {
    case FIELD_TYPE_INTEGER:
      encode_integer(encoder, FIELD_TYPE_INTEGER);
      encode_integer(encoder, v->v.i);
      break;

    case FIELD_TYPE_NUMBER:
      encode_integer(encoder, FIELD_TYPE_NUMBER);
      encode_number(encoder, v->v.n);
      break;

    case FIELD_TYPE_STRING:
      encode_integer(encoder, FIELD_TYPE_STRING);
      encode_string(encoder, &v->v.s);
      break;

    case FIELD_TYPE_UNKNOWN:
      encode_integer(encoder, FIELD_TYPE_UNKNOWN);
      break;

    case FIELD_TYPE_CUSTOM:
      old_b = encoder->b;
      old_ud = encoder->ud;
      encoder->ud = v->v.a;
      encoder->b = (sge_block *)sge_get_dict(&encoder->p->blocks, f->t_name,
                                             strlen(f->t_name));
      encode_integer(encoder, FIELD_TYPE_CUSTOM);
      ret = do_encode(encoder);
      encoder->b = old_b;
      encoder->ud = old_ud;
      break;

    default:
      SGE_PROTO_ERROR_ARG(encoder->p, SGE_ERR_ENCODE_ERROR,
                          "unknown field(%s:%s) type(%d).", encoder->b->name,
                          f->name, v->t);
  }

  return ret;
}

static int encode_by_key(sge_encoder *encoder, sge_field *f,
                         struct sge_key *k) {
  int ret = 0;
  sge_value v;
  uint16_t t = SGE_FIELD_TYPE(f->t);

  k->name.s = f->name;
  k->name.l = strlen(f->name);
  if (f->t_name) {
    k->t_name.s = f->t_name;
    k->t_name.l = strlen(f->t_name);
  }
  ret = encoder->get(encoder->ud, k, &v);
  if (SGE_OK != ret) {
    SGE_PROTO_ERROR_ARG(encoder->p, SGE_ERR_ENCODE_ERROR,
                        "get field(%s:%s) error.", encoder->b->name, f->name);
    return ret;
  }

  if (f->flags & FLAG_REQUIRED) {
    // required but got nil
    if (v.t == FIELD_TYPE_UNKNOWN) {
      SGE_PROTO_ERROR_ARG(encoder->p, SGE_ERR_ENCODE_ERROR,
                          "required field(%s:%s) but got nil.",
                          encoder->b->name, f->name);
      return SGE_ERR;
    }

    if (v.t != t) {
      SGE_PROTO_ERROR_ARG(encoder->p, SGE_ERR_ENCODE_ERROR,
                          "field(%s:%s) type not match expect %d got %d.",
                          encoder->b->name, f->name, t, v.t);
      return SGE_ERR;
    }
  }

  return encode_value(encoder, f, &v);
}

static int encode_normal(sge_encoder *encoder, sge_field *f) {
  struct sge_key k;
  k.t = KT_STRING;

  return encode_by_key(encoder, f, &k);
}

static int encode_list(sge_encoder *encoder, sge_field *f) {
  int idx = 0;
  int ret = 0;
  sge_value v;
  struct sge_key k;

  // first: get list size
  k.t = KT_STRING;
  k.name.s = f->name;
  k.name.l = strlen(f->name);
  ret = encoder->get(encoder->ud, &k, &v);
  if (ret != SGE_OK) {
    SGE_PROTO_ERROR_ARG(encoder->p, SGE_ERR_ENCODE_ERROR,
                        "get field(%s:%s) element size errors",
                        encoder->b->name, f->name);
    return SGE_ERR;
  }
  if (f->flags & FLAG_REQUIRED) {
    if (v.t != FIELD_TYPE_INTEGER) {
      SGE_PROTO_ERROR_ARG(
          encoder->p, SGE_ERR_ENCODE_ERROR,
          "get field(%s:%s) element size error. value type must be integer.",
          encoder->b->name, f->name);
      return SGE_ERR;
    }
  }
  encode_value(encoder, f, &v);

  if (v.t == FIELD_TYPE_INTEGER) {
    k.t = KT_LIST_INDEX;
    for (idx = 0; idx < v.v.i; ++idx) {
      k.idx = idx;
      ret = encode_by_key(encoder, f, &k);
      if (ret != SGE_OK) {
        return SGE_ERR;
      }
    }
  }

  return SGE_OK;
}

static int encode_field(sge_encoder *encoder, sge_field *f) {
  if (SGE_IS_LIST(f->t)) {
    return encode_list(encoder, f);
  } else {
    return encode_normal(encoder, f);
  }
}

static int do_encode(sge_encoder *encoder) {
  int i = 0;
  int ret = SGE_OK;
  sge_field *f = NULL;
  sge_block *b = encoder->b;

  encode_integer(encoder, b->id);
  for (i = 0; i < b->nf; ++i) {
    f = &b->fields[i];
    if ((ret = encode_field(encoder, f)) != SGE_OK) {
      break;
    }
  }

  return ret;
}

static sge_integer decode_integer(sge_decoder *decoder) {
  unsigned char c = 0;
  sge_integer i = 0;
  unsigned sge_integer mask = 0;
  int bit = 0;

  while (bit < SGE_INTEGER_SIZE && decoder->cursor < decoder->s.l) {
    c = (unsigned char)decoder->s.s[decoder->cursor];
    mask = c;
    mask <<= (bit * 8);
    i |= mask;
    bit += 1;
    decoder->cursor += 1;
  }

  return i;
}

static sge_number decode_number(sge_decoder *decoder) {
  sge_unused(decoder);

  return 0;
}

static size_t decode_string(sge_decoder *decoder, sge_string *s) {
  sge_integer len = 0;

  len = decode_integer(decoder);

  s->l = len;
  s->s = decoder->s.s + decoder->cursor;
  decoder->cursor += len;

  return len;
}

static int decode_normal(sge_decoder *decoder, sge_field *f,
                         struct sge_key *k) {
#define encode_type(type)                                                 \
  {                                                                       \
    sge_integer __t = decode_integer(decoder);                            \
    if (__t != (type)) {                                                  \
      if (f->flags & FLAG_REQUIRED) {                                     \
        SGE_PROTO_ERROR_ARG(                                              \
            decoder->p, SGE_ERR_ENCODE_ERROR,                             \
            "field(%s:%s) type not match decode value(%lld) expect(%d).", \
            decoder->b->name, f->name, __t, type);                        \
        return SGE_ERR;                                                   \
      } else {                                                            \
        v.t = __t;                                                        \
        break;                                                            \
      }                                                                   \
    }                                                                     \
  }

  int ret = SGE_OK;
  void *ud = NULL, *old_ud = NULL;
  uint16_t t = SGE_FIELD_TYPE(f->t);
  sge_value v;

  fill_key(k, f);
  v.t = t;
  switch (t) {
    case FIELD_TYPE_INTEGER:
      encode_type(FIELD_TYPE_INTEGER);
      v.v.i = decode_integer(decoder);
      break;

    case FIELD_TYPE_NUMBER:
      encode_type(FIELD_TYPE_NUMBER);
      v.v.n = decode_number(decoder);
      break;

    case FIELD_TYPE_STRING:
      encode_type(FIELD_TYPE_STRING);
      decode_string(decoder, &v.v.s);
      break;

    case FIELD_TYPE_UNKNOWN:
      encode_type(FIELD_TYPE_UNKNOWN);
      v.t = FIELD_TYPE_UNKNOWN;
      break;

    case FIELD_TYPE_CUSTOM:
      encode_type(FIELD_TYPE_CUSTOM);
      if (NULL == (ud = decoder->set(decoder->ud, k, &v))) {
        return SGE_ERR;
      }
      old_ud = decoder->ud;
      decoder->ud = ud;
      ret = do_decode(decoder);
      decoder->ud = old_ud;
      return ret;

    default:
      SGE_PROTO_ERROR_ARG(decoder->p, SGE_ERR_ENCODE_ERROR,
                          "unknown field(%s:%s) type(%d).", decoder->b->name,
                          f->name, v.t);
      return SGE_ERR;
  }

  if (NULL == decoder->set(decoder->ud, k, &v)) {
    return SGE_ERR;
  }

  return SGE_OK;
}

static int decode_list(sge_decoder *decoder, sge_field *f) {
  int ret = SGE_ERR;
  sge_value v;
  struct sge_key k;
  sge_integer i = 0, len = 0, type = 0;
  void *ud = NULL, *old_ud = NULL;

  k.t = KT_STRING;
  fill_key(&k, f);

  type = decode_integer(decoder);
  if (type == FIELD_TYPE_UNKNOWN) {
    v.t = FIELD_TYPE_UNKNOWN;
    decoder->set(decoder->ud, &k, &v);
    return SGE_OK;
  }

  if (type == FIELD_TYPE_INTEGER) {
    len = decode_integer(decoder);
    v.t = f->t;
    v.v.i = len;
    if (NULL == (ud = decoder->set(decoder->ud, &k, &v))) {
      SGE_PROTO_ERROR_ARG(decoder->p, SGE_ERR_ENCODE_ERROR,
                          "get field(%s:%s) error.", decoder->b->name, f->name);
      return SGE_ERR;
    }

    old_ud = decoder->ud;
    decoder->ud = ud;
    k.t = KT_LIST_INDEX;
    for (i = 0; i < len; ++i) {
      k.idx = i;
      if (SGE_OK != (ret = decode_normal(decoder, f, &k))) {
        return ret;
      }
    }
    decoder->ud = old_ud;
  }

  return ret;
}

static int decode_field(sge_decoder *decoder, sge_field *f) {
  uint16_t is_arr = SGE_IS_LIST(f->t);
  struct sge_key k;

  if (is_arr) {
    return decode_list(decoder, f);
  } else {
    k.t = KT_STRING;
    return decode_normal(decoder, f, &k);
  }
}

static int do_decode(sge_decoder *decoder) {
  size_t i = 0;
  sge_integer bid = 0;
  sge_proto *p = decoder->p;
  sge_block *bp = NULL, *old_bp = NULL;
  sge_field *f = NULL;

  bid = decode_integer(decoder);
  if (bid > p->max_bid) {
    SGE_PROTO_ERROR_ARG(p, SGE_ERR_DECODE_ERROR,
                        "bid(%lld) greater than max bid(%lld)", bid,
                        p->max_bid);
    return SGE_ERR;
  }

  bp = p->block_arr[bid - 1];
  if (NULL == bp) {
    SGE_PROTO_ERROR_ARG(p, SGE_ERR_DECODE_ERROR,
                        "not found block by bid(%lld).", bid);
    return SGE_ERR;
  }

  old_bp = decoder->b;
  decoder->b = bp;
  for (i = 0; i < bp->nf; ++i) {
    f = &bp->fields[i];
    if (SGE_OK != decode_field(decoder, f)) {
      return SGE_ERR;
    }
  }
  decoder->b = old_bp;

  return SGE_OK;
}

struct sge_string *sge_encode(struct sge_proto *p, const char *name,
                              const void *ud, sge_fn_get fn_get) {
  sge_block *bp = NULL;
  sge_encoder encoder;
  const char ver[] = {SGE_PROTO_VERSION, '\0'};

  if (NULL == p || NULL == name) {
    return NULL;
  }

  bp = (sge_block *)sge_get_dict(&p->blocks, name, strlen(name));
  if (NULL == bp) {
    SGE_PROTO_ERROR(p, SGE_ERR_BLOCK_NAME_NOT_FOUND);
    return NULL;
  }

  encoder.b = bp;
  encoder.get = fn_get;
  encoder.p = p;
  encoder.ud = ud;
  encoder.result = NULL;
  append_string(&encoder, ver, 1);
  append_string(&encoder, "  ", 2);  // crc16 placehold

  if (SGE_OK == do_encode(&encoder)) {
    compress(&encoder);
    set_crc16(&encoder);
    return &encoder.result->s;
  } else {
    destroy_string(encoder.result);
    return NULL;
  }
}

int sge_decode(struct sge_proto *p, const char *s, size_t len, void *ud,
               sge_fn_set fn_set) {
  int ret = SGE_OK;
  char real_buf[UNCOMPRESS_OUTPUT_LEN];
  ssize_t real_len = 0;
  sge_decoder decoder;

  if (NULL == p || NULL == s || len <= 0) {
    return SGE_ERR;
  }

  if (s[0] != SGE_PROTO_VERSION) {
    SGE_PROTO_ERROR_ARG(p, SGE_ERR_DECODE_ERROR,
                        "version not match expect %d, got %d",
                        SGE_PROTO_VERSION, s[0]);
    return SGE_ERR;
  }
  if (SGE_OK != check_crc(s, len)) {
    SGE_PROTO_ERROR_ARG(p, SGE_ERR_DECODE_ERROR, "crc not match");
    return SGE_ERR;
  }

  real_len = uncompress(s, len, real_buf);
  decoder.b = NULL;
  decoder.cursor = 3;
  decoder.p = p;
  decoder.s.s = real_buf;
  decoder.s.l = real_len;
  decoder.set = fn_set;
  decoder.ud = ud;
  ret = do_decode(&decoder);

  return ret;
}

void sge_free_string(struct sge_string *s) {
  if (NULL == s) {
    return;
  }

  if (s->s) {
    sge_free(s->s);
  }

  sge_free(s);
}
