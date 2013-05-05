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
#include <penny/debug.h>
#include <penny/mem.h>

#include <ccan/container_of/container_of.h>
#include <ccan/net/net.h>
#include <ccan/err/err.h>
#include <ccan/compiler/compiler.h>
#include <ccan/str/str.h>
#include <ccan/array_size/array_size.h>

#include <ev.h>

#include "irc.h"

struct conn {
	ev_io w;

	/* things the server could potentially change from what I set them to.
	 */
	int user_mode;
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

static char *irc_parse_prefix(char *start, size_t len, char **prefix, size_t *prefix_len)
{
	if (len <= 0 || *start != ':') {
		*prefix = NULL;
		*prefix_len = 0;
		return start;
	}

	*prefix = start + 1;

	/* the pkt starts with a nick or server name */
	char *next = memchr(start + 1, ' ', len - 1);
	if (!next) {
		warnx("invalid packet: couldn't locate a space after the first ':name'");
		return NULL;
	}

	*prefix_len = next - *prefix;

	/* point to the thing after the space */
	/* next + 1 :: skip the space we found with memchr */
	return memnchr(next + 1, ' ', len - (next + 1 - start));
}

#define assign_goto(var, val, label) do { \
		(var) = (val);		\
		goto label;		\
	} while (0)
static int process_pkt(struct conn *c, char *start, size_t len)
{
	if (!len)
		return -EMSGSIZE;

	char *prefix;
	size_t prefix_len;
	char *command = irc_parse_prefix(start, len, &prefix, &prefix_len);

	if (!command)
		return -EINVAL;

	char *remain = memchr(command + 1, ' ', len - (command + 1 - start));
	size_t command_len = remain - command;
	/* skip duplicate spaces */
	remain = memnchr(remain + 1, ' ', len - (remain + 1 - start));

	pr_debug(1, "prefix=\"%.*s\", command=\"%.*s\", remain=\"%.*s\"\n",
			prefix_len, prefix,
			command_len, command,
			len - (remain - start), remain);

	if (isalpha(*command)) {
		if (memeq(command, command_len, "PING", 4)) {
			char *p = start + 5;
			/* XXX: ensure @p has a server spec. */
			send_irc_cmd(c, "PONG %.*s", (int)(len - 5), p);
			return 0;
		} else {
			printf("unhandled command %.*s\n",
					command_len, command);
			return 1;
		}
	} else if (command_len == 3 && isdigit(*command)
				&& isdigit(*(command+1))
				&& isdigit(*(command+2))) {
		char const *name;
		int cmd_val = (*(command) - '0') * 100
			+ (*(command + 1) - '0') * 10
			+ (*(command + 2) - '0');
		if (cmd_val >= ARRAY_SIZE(irc_num_cmds))
			name = "(out of bounds)";
		else
			name = irc_num_cmds[cmd_val];

		if (!name)
			name = "(unknown)";

		switch(cmd_val) {
			case 372: /* MOTD */
			case 376: /* ENDOFMOTD */
			case 375: /* MOTDSTART */
				return 0;
			default:
				printf("unhandled numeric command: %d %s\n",
						cmd_val, name);
				return 1;
		}
	} else {
		warnx("invalid packet: unparsable command.");
		return -EINVAL;
	}

	return 0;
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

		r = process_pkt(c, start, len);
		if (r) {
			printf("> %zd ", len);
			print_bytes_as_cstring(start, len, stdout);
			putchar('\n');
		}

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
	send_irc_cmd(c, "USER %s hostname servername :%s",
			c->user, c->realname);
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

	freeaddrinfo(res);

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
