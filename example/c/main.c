#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sge_proto.h"

int main(int argc, char const *argv[]) {
	int code = sge_parse("	\
bb 2 {						\
	cc aa;					\
	ww dd;					\
}							\
aa 1 {						\
	aa number8;				\
}							\
dd 3 {						\
	dd number8;				\
}							\
");
	printf("code: %d\n", code);
	printf("parse error: %s\n", sge_error(code));
	sge_print();
	sge_destroy(1);
	return 0;
}
