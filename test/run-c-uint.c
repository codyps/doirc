
#include "parse-c-struct-izl.c"

#define __stringify_1(s) #s
#define __stringify(s) __stringify_1(s)

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "penny/print.h"
#include "penny/mem.h"

#include <unistd.h>

int main(void)
{
	uintmax_t v;
	size_t err_ct = 0;

#define P(s) parse_uint(s, strlen(s), &v)

#define C_(to_parse, expected_v) do {					\
	ssize_t __C_p = P(to_parse);						\
	printf("PARSE(");						\
	print_bytes_as_cstring(to_parse, sizeof(to_parse) - 1, stdout);	\
	printf(") %zd\n", __C_p);					\
	if (__C_p > 0)							\
		printf("EQ? EXPECTED: %ju == PARSED: %ju : %s\n",				\
				(uintmax_t)expected_v, v, expected_v == v ? "yes" : "NO!!!");	\
} while (0)


#define C(v) C_(__stringify(v), v)

	C(3);
	C(11241);

	return err_ct;
}

