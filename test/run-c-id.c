
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
	ssize_t p;
	char *out;
	size_t err_ct = 0;

#define P(s) parse_id(s, strlen(s), &out)

#define MEM_EQ(a, a_len, b, b_len) do {			\
	printf(">> ");					\
	print_bytes_as_cstring(a, a_len, stdout);		\
	printf(" = ");					\
	print_bytes_as_cstring(b, b_len, stdout);		\
	bool __MEM_EQ = memeq(a, a_len, b, b_len);	\
	printf(": %s\n", __MEM_EQ ? "yes" : "NO!!!");	\
	err_ct++;					\
} while (0)

#define ARRAY_EQ_MEM(a, b, b_len) do {		\
	MEM_EQ(a, sizeof(a) - 1, b, b_len);		\
} while (0)

/*
 * @s: parse OUTPUT
 * @ss: string to parse
 */
#define C_(s, ss) do {							\
	ssize_t __C_p = P(ss);						\
	printf("PARSE(");						\
	print_bytes_as_cstring(ss, sizeof(ss) - 1, stdout);		\
	printf(") %zd\n", __C_p);					\
	if (__C_p > 0)							\
		ARRAY_EQ_MEM(s, out, __C_p);				\
} while (0)

	C_("id", "id foo");
	C_("id", "id.foo");
	C_("id", "id\nfoo");
	C_("", "");
	C_(" ", " ");

	return err_ct;
}

