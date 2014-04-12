
#include "parse-c-struct-izl.c"

#define __stringify_1(s) #s
#define __stringify(s) __stringify_1(s)

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "penny/print.h"
#include "penny/mem.h"

#include <unistd.h>


static uintmax_t v;
static const char *id;
static size_t id_len;
static char str[1024];
static size_t str_len;
static enum type {
	TYPE_INVALID,
	TYPE_STR,
	TYPE_UINT,
} type;

static int cb_uint(struct c_ilz_ctx *i, const char *id_, size_t id_len_, uintmax_t v_)
{
	id = id_;
	id_len = id_len_;
	v = v_;
	type = TYPE_UINT;
	return 0;
}

static int cb_str(struct c_ilz_ctx *i, const char *id_, size_t id_len_, const char *str_, size_t str_len_)
{
	id = id_;
	id_len = id_len_;
	memcpy(str, str_, sizeof(str));
	str_len = str_len_;
	type = TYPE_STR;
}

int main(void)
{
	ssize_t p;
	size_t err_ct = 0;
	struct c_ilz_ctx ctx = {
		.parse_string = cb_str,
		.parse_uint = cb_uint,
	};

#define P(s) parse_elem(&ctx, s, strlen(s))

#define MEM_EQ(a, a_len, b, b_len) do {			\
	printf(">> ");					\
	print_bytes_as_cstring(a, a_len, stdout);		\
	printf(" = ");					\
	print_bytes_as_cstring(b, b_len, stdout);		\
	bool __MEM_EQ = memeq(a, a_len, b, b_len);	\
	printf(": %s\n", __MEM_EQ ? "yes" : "NO!!!");	\
	err_ct++;					\
} while (0)

#define STRLIT_EQ_MEM(a, b, b_len) do {		\
	MEM_EQ(a, sizeof(a) - 1, b, b_len);		\
} while (0)

#define DO_P(to_parse)			\
	type = TYPE_INVALID;		\
	ssize_t __C_p = P(to_parse);	\
	printf("PARSE(");						\
	print_bytes_as_cstring(to_parse, sizeof(to_parse) - 1, stdout);	\
	printf(") %zd\n", __C_p)

#define EXPECT_EQ(a, b) do {							\
	printf("EQ? %ju == %ju : %s\n", (uintmax_t)a, (uintmax_t)b, (a) == (b) ? "yes" : "NO!!!");	\
	if ((a) != (b))	{							\
		err_ct++;							\
	}									\
} while (0)

#define C_I(to_parse, expected_id, expected_val) do {			\
	DO_P(to_parse);							\
	if (__C_p > 0) {						\
		EXPECT_EQ(type, TYPE_UINT);				\
		STRLIT_EQ_MEM(expected_id, id, id_len);			\
		printf(">> EXPECTED: %ju == PARSED: %ju ? %s\n",		\
				(uintmax_t)expected_val, v, expected_val == v ? "yes" : "NO!!!");	\
	}								\
} while (0)

#define C_S(to_parse, expected_id, expected_str) do {			\
	DO_P(to_parse);							\
	if (__C_p > 0) {						\
		EXPECT_EQ(type, TYPE_STR);				\
		STRLIT_EQ_MEM(expected_id, id, id_len);			\
		STRLIT_EQ_MEM(expected_str, str, str_len);		\
	}								\
} while (0)

	C_I(".foo=3", "foo", 3);
	C_S(".bar=\"str\"", "bar", "str");

#if 0
	/* Failure expected */
	C__(".bar.=3");
	C__("bar=3");
	C__(".bar=.3");
#endif

	return err_ct;
}

