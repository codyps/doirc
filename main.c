#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdarg.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <penny/print.h>
#include <penny/penny.h>

#include <ccan/container_of/container_of.h>
#include <ccan/net/net.h>
#include <ccan/err/err.h>
#include <ccan/compiler/compiler.h>
#include <ccan/str/str.h>

#include <ev.h>

#include "irc.h"

struct conn {
	ev_io w;

	/* things the server could potentially change from what I set them to.
	 */
	int mode;
	const char *nick;

	const char *server_name;

	/* We need these to connect, put them here. */
	const char *realname;
	const char *user;
	const char *pass;

	size_t in_pos;
	char in_buf[2048];
};

static void send_irc_cmd(struct conn *c, char const *str, ...)
{
	char buf[2048];
	va_list va;
	va_start(va, str);
	ssize_t sz = vsnprintf(buf, sizeof(buf), str, va);
	va_end(va);
	sz += snprintf(buf + sz, sizeof(buf) - sz, "\r\n");

	ssize_t r = write(c->w.fd, buf, sz);
	if (r != sz)
		warnx("write failed: had %zu, sent %zu",
				sz, r);

	printf("< %zd ", r);
	print_bytes_as_cstring(buf, r, stdout);
	putchar('\n');
}

static void *memmem(const void *haystack, size_t haystacklen,
	     const void *needle, size_t needlelen)
{
	const char *p, *last;
	if (!haystacklen)
		return NULL;

	p = haystack;
	last = p + haystacklen - needlelen;

	do {
		if (memcmp(p, needle, needlelen) == 0)
			return (void *)p;
	} while (p++ <= last);

	return NULL;
}

static bool memstarts(void const *data, size_t data_len,
		void const *prefix, size_t prefix_len)
{
	if (prefix_len > data_len)
		return false;
	return !memcmp(data, prefix, prefix_len);
}

static void process_pkt(struct conn *c, char *start, size_t len)
{
	if (!len)
		return;

	if (*start == ':') {
		/* the pkt starts with a nick or server name */

		if (!c->server_name) {
			/* Assume that the first message is from the server we
			 * are connected to */
			c->server_name = strdup(strsep(&start, " "));
		}

		return;
	}

	if (memstarts(start, len, "PING ", 5)) {
		char *p = start + 5;
		/* XXX: ensure @p has a server spec. */
		send_irc_cmd(c, "PONG %.*s", (int)(len - 5), p);
		return;
	}
}

/* general fmt of messages */
/* :server_from number_status yournick :junk */
static void conn_cb(EV_P_ ev_io *w, int revents)
{
	struct conn *c = container_of(w, typeof(*c), w);

	ssize_t max_read = sizeof(c->in_buf) - c->in_pos;
	char *c_buf = c->in_buf + c->in_pos;

	if (max_read <= 0)
		warnx("buffer overflow, discarding.");

	ssize_t r = read(w->fd, c_buf, max_read);
	if (r == 0) {
		fputs("server closed link, exiting...\n", stdout);
		ev_io_stop(EV_A_ w);
		ev_break(EV_A_ EVBREAK_ALL);
		return;
	}

	if (r == -1) {
		warn("failed to read");
		return;
	}

	c->in_pos += r;

#if 0
	printf("R %zd ", c->in_pos);
	print_bytes_as_cstring(c->in_buf, c->in_pos, stdout);
	putchar('\n');
	printf("B %zd ", c->in_pos);
	print_bytes_as_cstring(c->in_buf, c->in_pos, stdout);
	putchar('\n');
#endif

	char *end   = memmem(c_buf, r, "\r\n", 2);
	char *start = c->in_buf;
	size_t buf_len = c->in_pos;
	for (;;) {
		if (!end)
			break;

		size_t len = end - start;

		printf("> %zd ", len);
		print_bytes_as_cstring(start, len, stdout);
		putchar('\n');

		process_pkt(c, start, len);

		start = end + 2;
		buf_len -= len + 2;
		end = memmem(start, buf_len, "\r\n", 2);
	}

	memmove(c->in_buf, start, buf_len);
	c->in_pos = buf_len;

#if 0
	printf("B %zd ", c->in_pos);
	print_bytes_as_cstring(c->in_buf, c->in_pos, stdout);
	putchar('\n');
#endif
}

static void irc_connect(struct conn *c)
{
	if (c->pass)
		send_irc_cmd(c, "PASS %s", c->pass);
	send_irc_cmd(c, "NICK %s", c->nick);
	send_irc_cmd(c, "USER %s %d %s %s", c->user, c->mode,
			"ignore", c->realname);
}

int main(int argc, char **argv)
{
	err_set_progname(argv[0]);
	if (argc != 3) {
		fprintf(stderr, "usage: %s <server> <port>\n", argv[0]);
		return -1;
	}


	struct addrinfo *res = net_client_lookup(argv[1], argv[2],
			AF_UNSPEC, SOCK_STREAM);
	if (!res)
		err(1, "resolve failure\n");

	int fd = net_connect(res);
	if (fd == -1)
		err(1, "connection failed\n");

	struct conn conn = {
		.nick = "bye555",
		.user = "bye555",
		.realname = "bye555",
	};

	ev_io_init(&conn.w, conn_cb, fd, EV_READ);
	ev_io_start(EV_DEFAULT, &conn.w);

	irc_connect(&conn);

	ev_run(EV_DEFAULT, 0);
	return 0;
}
