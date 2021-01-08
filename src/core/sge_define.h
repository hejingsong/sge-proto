#ifndef SGE_DEFINE_H_
#define SGE_DEFINE_H_

#include <stdlib.h>
#include <stdint.h>


#define SGE_OK	0
#define SGE_ERR	-1

#define MIN_ERROR_CODE		0
#define INVALID_PARAM		-2
#define RES_CANT_ACCESS		-3
#define NOT_SCHEME			-4
#define MAX_ERROR_CODE		4

#define sge_malloc	malloc
#define sge_free	free

typedef enum sge_value_type {
	SGE_NUMBER = 1,
	SGE_STRING,
	SGE_LIST,
	SGE_DICT
} sge_value_type;

typedef struct sge_value {
	const void *ptr;
	const char *name;
	size_t len;
	int32_t idx;
	sge_value_type vt;
} sge_value;

typedef void (*field_get)(const void *, sge_value *);
typedef void* (*field_set)(void *, sge_value *);

#define NEW_SGE_VALUE	{NULL, NULL, 0, -1, 1}


#endif
