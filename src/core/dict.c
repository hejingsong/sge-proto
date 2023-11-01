#include <string.h>

#include "core.h"

static size_t hash_str(const char *s, size_t l) {
  const char *pkey = (const char *)s;
  register unsigned long hash = 5381;

  for (; l >= 8; l -= 8) {
    hash = ((hash << 5) + hash) + *pkey++;
    hash = ((hash << 5) + hash) + *pkey++;
    hash = ((hash << 5) + hash) + *pkey++;
    hash = ((hash << 5) + hash) + *pkey++;
    hash = ((hash << 5) + hash) + *pkey++;
    hash = ((hash << 5) + hash) + *pkey++;
    hash = ((hash << 5) + hash) + *pkey++;
    hash = ((hash << 5) + hash) + *pkey++;
  }

  switch (l) {
    case 7:
      hash = ((hash << 5) + hash) + *pkey++;  // fall through
    case 6:
      hash = ((hash << 5) + hash) + *pkey++;  // fall through
    case 5:
      hash = ((hash << 5) + hash) + *pkey++;  // fall through
    case 4:
      hash = ((hash << 5) + hash) + *pkey++;  // fall through
    case 3:
      hash = ((hash << 5) + hash) + *pkey++;  // fall through
    case 2:
      hash = ((hash << 5) + hash) + *pkey++;  // fall through
    case 1:
      hash = ((hash << 5) + hash) + *pkey++;
      break;
    case 0:
      break;
    default:
      break;
  }

  return hash;
}

static size_t hash(const char *key, size_t kl) { return hash_str(key, kl); }

static int key_compare(const char *k1, size_t kl1, const char *k2, size_t kl2) {
  int len = (kl1 > kl2) ? kl1 : kl2;

  return strncmp((const char *)k1, (const char *)k2, len);
}

static sge_dict_node *alloc_node(const char *k, size_t kl, void *data) {
  sge_dict_node *n = NULL;

  n = (sge_dict_node *)sge_malloc(sizeof(*n));
  n->k = k;
  n->kl = kl;
  n->data = data;
  SGE_LIST_INIT(&n->entry);

  return n;
}

void sge_init_dict(sge_dict *d) {
  int i = 0;
  struct list *l;

  if (NULL == d) {
    return;
  }

  for (i = 0; i < SGE_DICT_SLOT_SIZE; ++i) {
    l = &d->slots[i];
    SGE_LIST_INIT(l);
  }
}

void sge_insert_dict(sge_dict *d, const char *k, size_t kl, void *data) {
  size_t h = 0, found = 0;
  struct list *l, *iter = NULL;
  sge_dict_node *n = NULL;

  if (NULL == d || NULL == k || kl == 0 || NULL == data) {
    return;
  }

  h = hash(k, kl) % SGE_DICT_SLOT_SIZE;
  l = &d->slots[h];

  SGE_LIST_FOREACH(iter, l) {
    n = sge_container_of(iter, sge_dict_node, entry);
    if (0 == key_compare(n->k, n->kl, k, kl)) {
      found = 1;
      break;
    }
  }

  if (found) {
    n->data = data;
  } else {
    n = alloc_node(k, kl, data);
    SGE_LIST_APPEND(l, &n->entry);
  }
}

void *sge_get_dict(sge_dict *d, const char *k, size_t kl) {
  size_t h = 0, found = 0;
  struct list *l = NULL, *iter = NULL;
  sge_dict_node *n = NULL;

  h = hash(k, kl) % SGE_DICT_SLOT_SIZE;
  l = &d->slots[h];

  SGE_LIST_FOREACH(iter, l) {
    n = sge_container_of(iter, sge_dict_node, entry);
    if (0 == key_compare(n->k, n->kl, k, kl)) {
      found = 1;
      break;
    }
  }

  if (found) {
    return n->data;
  } else {
    return NULL;
  }
}

void sge_del_dict(sge_dict *d, const char *k, size_t kl) {
  size_t h = 0;
  struct list *l = NULL, *iter = NULL, *next = NULL;
  sge_dict_node *n = NULL;

  h = hash(k, kl) % SGE_DICT_SLOT_SIZE;
  l = &d->slots[h];

  SGE_LIST_FOREACH_SAFE(iter, next, l) {
    n = sge_container_of(iter, sge_dict_node, entry);
    if (0 == key_compare(n->k, n->kl, k, kl)) {
      SGE_LIST_REMOVE(&n->entry);
      sge_free(n);
      break;
    }
  }
}

void sge_free_dict(sge_dict *d) {
  size_t i = 0;
  struct list *l = NULL, *iter = NULL, *next = NULL;
  sge_dict_node *n = NULL;

  for (i = 0; i < SGE_DICT_SLOT_SIZE; ++i) {
    l = &d->slots[i];
    SGE_LIST_FOREACH_SAFE(iter, next, l) {
      n = sge_container_of(iter, sge_dict_node, entry);
      SGE_LIST_REMOVE(&n->entry);
      sge_free(n);
    }
  }
}

void sge_init_dict_iter(sge_dict_iter *iter, sge_dict *d) {
  if (NULL == iter || NULL == d) {
    return;
  }

  iter->d = d;
  iter->slot = 0;
  iter->node = d->slots[0].next;
}

void *sge_dict_iter_next(sge_dict_iter *iter) {
  sge_dict_node *n = NULL;

  if (NULL == iter || iter->slot >= SGE_DICT_SLOT_SIZE) {
    return NULL;
  }

  if (iter->node == &(iter->d->slots[iter->slot])) {
    while (iter->slot < SGE_DICT_SLOT_SIZE) {
      iter->slot++;
      if (iter->slot >= SGE_DICT_SLOT_SIZE ||
          SGE_LIST_EMPTY(&iter->d->slots[iter->slot])) {
        continue;
      }
      iter->node = iter->d->slots[iter->slot].next;
      break;
    }
    if (iter->slot >= SGE_DICT_SLOT_SIZE) {
      return NULL;
    }
  }

  n = sge_container_of(iter->node, sge_dict_node, entry);
  iter->node = iter->node->next;

  return n->data;
}
