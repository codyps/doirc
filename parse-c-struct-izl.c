#include "c-struct-izl.h"
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#define EXPECT(c) do {					\
	if (consumed >= len || buf[consumed] != c)	\
		return -1;				\
	consumed ++;					\
} while (0)

#define HAVE_BYTE() do {				\
	if (consumed > len)				\
		return -1;				\
} while (0)

#define R (len > consumed ? len - consumed : 0)
static ssize_t parse_id(const char *buf, size_t len, char **elem, size_t *elem_len)
{
	size_t consumed = 0;
	size_t el = 0;
	const char *e = buf + consumed;
	*elem = (char *)e;
	el = 0;

	HAVE_BYTE();
	/* must have at least 1 letter */
	if (!isalpha(e[el]))
		return -1;

	el ++;

	for (;;) {
		if (consumed > len)
			break;

		int b = e[el];
		if (!(isalpha(b) || isdigit(b) || strchr("$_", b)))
			break;
		el++;
	}

	*elem_len = el;
	return consumed;
}

static ssize_t parse_uint(const char *buf, size_t len, uintmax_t *v)
{
	uintmax_t u;
	size_t i;
	size_t consumed = 0;
	for (i = 0; i < len; i++) {
		if (isdigit(buf[i])) {
			u *= 10;
			u += buf[i] - '0';
		} else {
			*v = u;
			return i;
		}
	}
}

static int from_hex(int c)
{
	if ('a' <= c && c <= 'f')
		return c - 'a' + 10;
	else if ('A' <= c && c <= 'F')
		return c - 'A' + 10;
	else if ('0' <= c && c <= '9')
		return c - '0';
	else
		return -1;
}

static ssize_t parse_str(const char *buf, size_t len, char *out, size_t *out_len)
{
	if (len < 2)
		return -1;
	if (*buf != '"')
		return -2;

	bool esc = false;
	size_t i, o;
	for (i = 1, o = 0;; i++) {
		if (i >= len)
			/* no more input */
			return -3;
		if (o >= len)
			/* no more output */
			return -4;

		int c = buf[i];
		printf(">>> got %c\n", c);
		switch (c) {
		case '"':
			if (esc) {
				out[o] = '"';
				o++;
				esc = false;
			} else {
				*out_len = o;
				return i;
			}
			break;
		case '\\':
			esc = true;
			break;
		default:
			if (esc) {
				esc = false;
				switch (c) {
				case 'x':
					/* parse 2 digits */
					if ((len - i) < 2)
						return -1;
					int b = from_hex(buf[i + 1]);
					if (b < 0)
						return -1;
					out[o] = b << 4;
					b = from_hex(buf[i + 2]);
					if (b < 0)
						return -1;
					out[o] |= b;
					o++;
					i += 2;
					break;
				case 'n':
					out[o] = '\n';
					o++;
					break;
				case 'r':
					out[o] = '\r';
					o++;
					break;
				case '0':
					out[o] = '\0';
					o++;
					break;
				default:
					/* don't know how to handle this escape, fail */
					return -1;
				}
			}
		}
	}
}

static ssize_t parse_elem(struct c_struct_initializer *i, const char *buf, size_t len)
{
	ssize_t p;
	size_t consumed = 0;
	char *id;
	size_t id_len;
	char obuf[1024];
	size_t o;

	EXPECT('.');
	HAVE_BYTE();
	p = parse_id(buf + consumed, R, &id, &id_len);
	if (p < 0)
		return p;
	consumed += p;
	EXPECT('=');
	HAVE_BYTE();
	if (buf[consumed] == '"') {
		/* looks like a string */
		o = sizeof(buf);
		p = parse_str(buf + consumed, R, obuf, &o);
		if (p < 0)
			return p;
	} else if (isdigit(buf[consumed])) {
		/* number */
		uintmax_t v;
		p = parse_uint(buf + consumed, R, &v);
		if (p < 0)
			return p;
	} else {
		/* who knows */
		return -1;
	}

	return consumed;
}

ssize_t parse_c_struct_initializer(struct c_struct_initializer *i, const char *buf, size_t len)
{
	size_t consumed = 0;
	ssize_t p;
	EXPECT('{');

	for (;;) {
		HAVE_BYTE();

		p = parse_elem(i, buf + consumed, R);
		if (p < 0)
			return p;

		HAVE_BYTE();

		bool comma = false;
		if (buf[consumed] == ',') {
			/* could be the end or not */
			comma = true;
			consumed ++;
			HAVE_BYTE();
		}

		if (buf[consumed] == '}') {
			/* end */
			return consumed;
		}

		/* If we're continuing, we must have a comma */
		if (!comma)
			return -1;
	}
}
