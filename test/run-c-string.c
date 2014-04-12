#include "parse-c-struct-izl.c"

#define __stringify_1(s) #s
#define __stringify(s) __stringify_1(s)

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "penny/print.h"
#include "penny/mem.h"

int main(void)
{
	ssize_t p;
	char out[1024];
	size_t out_len;
	size_t err_ct = 0;

#define P(s) ({					\
	out_len = sizeof(out);			\
	parse_str(s, strlen(s), out, &out_len);	\
})

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

#define C_(s, ss) do {							\
	ssize_t __C_p = P(ss);						\
	printf("PARSE(");						\
	print_bytes_as_cstring(ss, sizeof(ss) - 1, stdout);		\
	printf(") %zd\n", __C_p);					\
	if (__C_p > 0)							\
		ARRAY_EQ_MEM(s, out, out_len);				\
} while (0)

#define C(s) C_(s, __stringify(s))

	C("hello");
	C("as\0as");
	C("buz\nbaz");
	C("");
	C("\r");
	C("foo\rbar");
	C_("foo\n", "\"foo\\n\"");

	return err_ct;
}
