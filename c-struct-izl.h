#ifndef C_STRUCT_IZL_H_
#define C_STRUCT_IZL_H_

#include <unistd.h>
#include <stdint.h>

struct c_struct_initializer {
	int (*parse_uint)(struct c_struct_initializer *i,
			const char *elem_name, size_t elem_name_len, uintmax_t v);
	int (*parse_string)(struct c_struct_initializer *i,
			const char *elem_name, size_t elem_name_len,
			const char *str, size_t len);
};

ssize_t parse_c_struct_initializer(struct c_struct_initializer *c,
		const char *buf, size_t len);

#endif

