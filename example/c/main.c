#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sge_proto.h"

int main(int argc, char const *argv[]) {
	int code = sge_parse("	\
aa 1 {						\
	aa number8;				\
}							\
bb 2 {						\
	cc aa;					\
}							\
");
	printf("code: %d\n", code);
	printf("parse error: %s\n", sge_error(code));
	sge_print();
	sge_destroy(1);
	return 0;
}
