#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>

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
#include <ccan/list/list.h>

#include <ev.h>

#include "irc.h"

int irc_cmd(struct irc_connection *c, char const *str, ...)
{
	char buf[1024];
	va_list va;
	va_start(va, str);
	ssize_t sz = vsnprintf(buf, sizeof(buf) - 2, str, va);
	va_end(va);
	buf[sz++] = '\r';
	buf[sz++] = '\n';

	ssize_t r = write(c->w.fd, buf, sz);
	if (r != sz)
		return (r < 0)?r:-1;

	if (debug_level() > 0) {
		printf("< %zd ", r);
		print_bytes_as_cstring(buf, r, stdout);
		putchar('\n');
	}

	return 0;
}

static int irc_parse_args(char const *start, size_t len, struct arg *args,
		size_t max_args)
{
	size_t arg_pos = 0;
	while (start && len && arg_pos < max_args) {
		if (*start == ':') {
			args[arg_pos].data = start + 1;
			args[arg_pos].len  = len - 1;
			return arg_pos + 1;
		}

		char *end_of_curr = memchr(start + 1, ' ', len - 1);
		if (!end_of_curr) {
			args[arg_pos].data = start;
			args[arg_pos].len  = len;
			return arg_pos + 1;
		}

		size_t len_of_curr = end_of_curr - start;

		args[arg_pos].data = start;
		args[arg_pos].len  = len_of_curr;

		//len -= next + 1 - start;
		char *start_of_next =
			memnchr(end_of_curr + 1, ' ', len - len_of_curr - 1);
		len -= start_of_next - start;
		start = start_of_next;
		arg_pos++;
	}

	if (len == 0)
		return arg_pos;

	return -1;
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

static struct arg first_comma_arg(const char *start, const char *end)
{
	const char *arg_end = memchr(start, ',', end - start);
	if (arg_end)
		return (struct arg){ start, arg_end - start };
	else
		return (struct arg){ start, end - start };
}

static struct arg next_comma_arg(struct arg a, const char *end)
{
	if (a.data + a.len >= end)
		return (struct arg) {0, 0};
	else
		return first_comma_arg(a.data + a.len + 1, end);
}

#define irc_for_each_comma_arg(arg, base_arg)		\
	for (arg = first_comma_arg(base_arg.data, base_arg.data + base_arg.len);	\
	     arg.len;	\
	     arg = next_comma_arg(arg, base_arg.data + base_arg.len))


static int handle_privmsg(struct irc_connection *c,
		char *prefix, size_t prefix_len,
		char *start, size_t len)
{
	struct arg args[2];
	int r = irc_parse_args(start, len, args, ARRAY_SIZE(args));
	if (r != ARRAY_SIZE(args)) {
		warnx("PRIVMSG requires exactly %zu arguments, got %d",
				ARRAY_SIZE(args), r);
		return -1;
	}

	pr_debug(1, "privmsg recipients: ");
	struct arg a;
	irc_for_each_comma_arg(a, args[0]) {
		pr_debug(1, "%.*s ", a.len, a.data);
	}
	pr_debug(1, "\n");

	pr_debug(1, "message contents: %.*s\n", args[1].len, args[1].data);

	/* FIXME: we only pass the last recipient */
	if (c->cb.privmsg)
		c->cb.privmsg(c, prefix, prefix_len,
			a.data, a.len,
			args[1].data, args[1].len);

	return 0;
}

bool irc_user_is_me(struct irc_connection *c, const char *start, size_t len)

{
	return memeq(c->user, strlen(c->user), start, len);
}

int irc_set_channel_user_mode(struct irc_connection *c,
		const char *channel, size_t channel_len,
		const char *name, size_t name_len,
		enum irc_channel_user_mode mode)
{
	if (!(mode & IRC_CUM_o))
		return 0;
	irc_cmd(c, "MODE %.*s +o %.*s", channel_len, channel,
			name_len, name);
	return 0;
}

int irc_clear_channel_user_mode(struct irc_connection *c,
		const char *channel, size_t channel_len,
		const char *name, size_t name_len,
		enum irc_channel_user_mode mode)
{
	if (!(mode & IRC_CUM_o))
		return 0;
	irc_cmd(c, "MODE %.*s -o %.*s", channel_len, channel,
			name_len, name);
	return 0;
}

static int handle_mode(struct irc_connection *c,
		const char *prefix, size_t prefix_len,
		const char *start, size_t len)
{
	struct arg args[3];
	int r = irc_parse_args(start, len, args, ARRAY_SIZE(args));
	/* TODO: support user modes (not channel modes) */
	if (r != ARRAY_SIZE(args)) {
		warnx("MODE requires exactly %zu arguments, got %d",
				ARRAY_SIZE(args), r);
		return -1;
	}

	if (c->cb.mode)
		c->cb.mode(c, prefix, prefix_len,
				args, r);
	return 0;
}

static int handle_rpl_namreply(struct irc_connection *c, char *start, size_t len)
{
	return 0;
}

static int handle_rpl_topic(struct irc_connection *c, char *start, size_t len)
{
	struct arg args[3];
	int r = irc_parse_args(start, len, args, ARRAY_SIZE(args));
	if (r != ARRAY_SIZE(args)) {
		warnx("RPL_TOPIC requires exactly %zu arguments, got %d",
				ARRAY_SIZE(args), r);
		return -1;
	}

	printf("topic set for \"%.*s\" in \"%.*s\" to \"%.*s\"\n",
			args[0].len, args[0].data,
			args[1].len, args[1].data,
			args[2].len, args[2].data);
	return 0;
}

int irc_cmd_invite(struct irc_connection *c,
		char const *nick, size_t nick_len,
		char const *chan, size_t chan_len)
{
	irc_cmd(c, "INVITE %.*s %.*s", nick, nick_len, chan, chan_len);
	return 0;
}

static int process_pkt(struct irc_connection *c, char *start, size_t len)
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
	size_t remain_len = len - (remain - start);

	pr_debug(1, "prefix=\"%.*s\", command=\"%.*s\", remain=\"%.*s\"",
			prefix_len, prefix,
			command_len, command,
			remain_len, remain);

	if (isalpha(*command)) {
		if (memeqstr(command, command_len, "PING")) {
			char *p = start + 5;
			/* XXX: ensure @p has a server spec. */
			irc_cmd(c, "PONG %.*s", (int)(len - 5), p);
			return 0;
		} else if (memeqstr(command, command_len, "PRIVMSG"))
			return handle_privmsg(c, prefix, prefix_len, remain, remain_len);
		else if (memeqstr(command, command_len, "MODE"))
			return handle_mode(c, prefix, prefix_len, remain, remain_len);
		else {
			printf("unhandled command %.*s\n",
					command_len, command);
			return 1;
		}
	} else if (command_len == 3 && isdigit(*command)
				&& isdigit(*(command+1))
				&& isdigit(*(command+2))) {
		char const *name;
		unsigned cmd_val = (*(command) - '0') * 100
			+ (*(command + 1) - '0') * 10
			+ (*(command + 2) - '0');
		if (cmd_val >= ARRAY_SIZE(irc_num_cmds))
			name = "(out of bounds)";
		else
			name = irc_num_cmds[cmd_val];

		if (!name)
			name = "(unknown)";

		switch(cmd_val) {
			case 353: /* NAMREPLY */
				return handle_rpl_namreply(c, remain, remain_len);
			case 332: /* TOPIC */
				return handle_rpl_topic(c, remain, remain_len);
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
	struct irc_connection *c = container_of(w, typeof(*c), w);

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

	if (debug_is(4)) {
		printf("R %zd ", c->in_pos);
		print_bytes_as_cstring(c->in_buf, c->in_pos, stdout);
		putchar('\n');
		printf("B %zd ", c->in_pos);
		print_bytes_as_cstring(c->in_buf, c->in_pos, stdout);
		putchar('\n');
	}

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

	if (debug_is(4)) {
		printf("B %zd ", c->in_pos);
		print_bytes_as_cstring(c->in_buf, c->in_pos, stdout);
		putchar('\n');
	}
}

int irc_cmd_join(struct irc_connection *c,
		char const *name, size_t name_len)
{
	irc_cmd(c, "JOIN %.*s", name, name_len);
	return 0;
}

bool irc_is_connected(struct irc_connection *c)
{
	return ev_is_active(&c->w);
}

static void irc_proto_connect(struct irc_connection *c)
{
	if (c->pass)
		irc_cmd(c, "PASS %s", c->pass);
	irc_cmd(c, "NICK %s", c->nick);
	irc_cmd(c, "USER %s hostname servername :%s",
			c->user, c->realname);
}

/*
 * return:
 *	a non-negative file descriptor on success
 *	-1 if dns resolution failed
 *	-2 if connection failed
 */
static int irc_net_connect(struct irc_connection *c)
{
	/* TODO: use cached dns info? */
	struct addrinfo *res = net_client_lookup(c->server, c->port,
			AF_UNSPEC, SOCK_STREAM);
	if (!res)
		return -1;

	int fd = net_connect(res);
	if (fd == -1)
		return -2;

	freeaddrinfo(res);

	/* FIXME: avoid depending on libev */
	ev_io_init(&c->w, conn_cb, fd, EV_READ);
	ev_io_start(EV_DEFAULT_ &c->w);

	return fd;
}

int irc_connect(struct irc_connection *c)
{
	int fd = irc_net_connect(c);
	if (fd < 0)
		return fd;
	irc_proto_connect(c);
	return fd;
}
