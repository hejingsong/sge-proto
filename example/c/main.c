#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sge/proto.h"

struct PhoneNumber {
  const char* id;
  int type[2];
};

struct Person {
  const char* name;
  int id;
  const char* email[2];
  struct PhoneNumber phone[2];
};

static const char* g_proto_file = "../example.proto";

static int get(const void* ud, const struct sge_key* k, struct sge_value* v) {
  if (0 == strncmp(k->name.s, "name", 4)) {
    sge_value_string(v, "name");
  } else if (0 == strncmp(k->name.s, "pid", 3)) {
    sge_value_integer(v, 1);
  } else if (0 == strncmp(k->name.s, "email", 5)) {
    if (k->t == KT_LIST_INDEX) {
      sge_value_string(v, "email");
    } else if (k->t == KT_STRING) {
      sge_value_integer(v, 2);
    }
  } else if (0 == strncmp(k->name.s, "phone", 5)) {
    if (k->t == KT_LIST_INDEX) {
      v->t = FIELD_TYPE_CUSTOM;
    } else if (k->t == KT_STRING) {
      sge_value_integer(v, 2);
    }
  }

  if (0 == strncmp(k->name.s, "id", 2)) {
    sge_value_string(v, "ssss");
  } else if (0 == strncmp(k->name.s, "type", 4)) {
    if (k->t == KT_STRING) {
      sge_value_integer(v, 10);
    } else if (k->t == KT_LIST_INDEX) {
      sge_value_integer(v, k->idx);
    }
  }

  return SGE_OK;
}

static int set(void* ud, const struct sge_key* k, const struct sge_value* v) {
  return SGE_OK;
}

int main(int argc, char const* argv[]) {
  struct sge_proto* proto = NULL;
  struct sge_string* result = NULL;
  const char* err = NULL;
  int err_code = 0;
  struct PhoneNumber phone_number1 = {.id = "aaaa", .type = {1, 2}};
  struct PhoneNumber phone_number2 = {.id = "bbbb", .type = {3, 4}};
  struct Person person = {.name = "phone",
                          .id = 1,
                          .email = {"email1", "email2"},
                          .phone = {phone_number1, phone_number2}};

  proto = sge_parse(g_proto_file);
  if (NULL == proto) {
    fprintf(stderr, "parse proto fail.\n");
    return -1;
  }

  err_code = sge_proto_error(proto, &err);
  printf("parser error: code(%d), error(%s)\n", err_code, err);

  if (err_code == 0) {
    sge_print_proto(proto);

    result = sge_encode(proto, "Person", &person, get);
    err_code = sge_proto_error(proto, &err);
    printf("parser error: code(%d), error(%s)\n", err_code, err);
    sge_decode(proto, result->s, result->l, &person, set);
  }

  sge_free_proto(proto);

  return 0;
}
