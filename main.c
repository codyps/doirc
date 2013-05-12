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

enum irc_user_mode {
	IRC_UM_i = 1 << 0,
	IRC_UM_w = 1 << 1,
	IRC_UM_s = 1 << 2,
	IRC_UM_o = 1 << 3,
};

enum irc_channel_user_mode {
	IRC_CUM_v = 1 << 0,
	IRC_CUM_o = 1 << 1,
};

enum irc_channel_mode {
	IRC_CM_s = 1 << 0,
};

struct user_in_channel {
	char *name;
	size_t name_len;
	enum irc_channel_user_mode mode;
};

struct irc_channel {
	struct list_node node;
	const char *name;
	size_t name_len;
	enum irc_channel_mode mode;
};

struct irc_client {
	ev_io w;

	/* network connection */
	const char *server;
	const char *port;
	struct addrinfo *addr;

	/* irc proto connection */
	const char *nick;
	const char *realname;
	const char *user;
	const char *pass;

	/* state while connected */
	enum irc_user_mode user_mode;
	struct list_head channels;

	/* buffers */
	size_t in_pos;
	char in_buf[2048];
};

static void irc_send_cmd(struct irc_client *c, char const *str, ...)
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

struct arg {
	const char *data;
	size_t len;
};

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


static int handle_privmsg(struct irc_client *c,
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

	printf("privmsg recipients: ");
	struct arg a;
	irc_for_each_comma_arg(a, args[0]) {
		printf("%.*s ", a.len, a.data);
	}
	putchar('\n');

	printf("message contents: %.*s\n", args[1].len, args[1].data);

	if (memeqstr(args[1].data, args[1].len, "!op")) {
		irc_set_channel_user_mode(c, 
	}


	return 0;
}

static struct irc_channel *get_channel_by_name(struct irc_client *c,
		const char *name, size_t len)
{
	struct irc_channel *ch;
	list_for_each(&c->channels, ch, node) {
		if (memeq(name, len, ch->name, ch->name_len))
			return ch;
	}

	return NULL;
}

static bool user_name_is_me(struct irc_client *c, const char *start, size_t len)

{
	return memeq(c->user, strlen(c->user), start, len);
}

static int irc_set_channel_user_mode(struct irc_client *c,
		struct irc_channel *ch, const char *name, size_t name_len,
		enum irc_channel_user_mode mode)
{
	if (!(mode & IRC_CUM_o))
		return 0;
	irc_send_cmd(c, "MODE %.*s +o %.*s", ch->name_len, ch->name,
			name_len, name);
	return 0;
}

static int irc_clear_channel_user_mode(struct irc_client *c,
		struct irc_channel *ch,
		const char *name, size_t name_len,
		enum irc_channel_user_mode mode)
{
	if (!(mode & IRC_CUM_o))
		return 0;
	irc_send_cmd(c, "MODE %.*s -o %.*s", ch->name_len, ch->name,
			name_len, name);
	return 0;
}

static int handle_mode(struct irc_client *c,
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

	struct irc_channel *ch = get_channel_by_name(c, args[0].data, args[0].len);
	if (!ch) {
		warnx("MODE refers to unknown channel %.*s",
				args[0].len, args[0].data);
		return -1;
	}

	if (*args[1].data == '-')
		return 0;

	/* FIXME: this is policy. Move to a callback */
	bool me = user_name_is_me(c, args[2].data, args[2].len);
	if (!me) {
		irc_clear_channel_user_mode(c, ch, args[2].data, args[2].len,
				IRC_CUM_o);
	}

	return 0;
}

static int handle_rpl_namreply(struct irc_client *c, char *start, size_t len)
{
	return 0;
}

static int handle_rpl_topic(struct irc_client *c, char *start, size_t len)
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

static int irc_invite(struct irc_client *c, char *nick, size_t nick_len,
		char *chan, size_t chan_len)
{
	irc_send_cmd(c, "INVITE %.*s %.*s", nick, nick_len, chan, chan_len);
	return 0;
}

static int process_pkt(struct irc_client *c, char *start, size_t len)
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
			irc_send_cmd(c, "PONG %.*s", (int)(len - 5), p);
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
	struct irc_client *c = container_of(w, typeof(*c), w);

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

static int _irc_add_channel(struct irc_client *c, char const *name, size_t name_len)
{
	struct irc_channel *ch = malloc(sizeof(*ch));
	if (!ch)
		return -ENOMEM;

	*ch = (typeof(*ch)) {
		.name = name,
		.name_len = name_len,
	};

	list_add(&c->channels, &ch->node);
	return 0;
}

static int irc_send_join(struct irc_client *c,
		char const *name, size_t name_len)
{
	irc_send_cmd(c, "JOIN %.*s", name, name_len);
	return 0;
}

static bool irc_is_connected(struct irc_client *c)
{
	return ev_is_active(&c->w);
}

#define irc_join_(c, n) irc_join(c, n, strlen(n))
static int irc_join(struct irc_client *c, char const *name, size_t name_len)
{
	int r = _irc_add_channel(c, name, name_len);
	if (r)
		return -1;

	if (irc_is_connected(c)) {
		return irc_send_join(c, name, name_len);
	} else {
		return 1;
	}
}

static void irc_proto_connect(struct irc_client *c)
{
	if (c->pass)
		irc_send_cmd(c, "PASS %s", c->pass);
	irc_send_cmd(c, "NICK %s", c->nick);
	irc_send_cmd(c, "USER %s hostname servername :%s",
			c->user, c->realname);
	struct irc_channel *chan;
	list_for_each(&c->channels, chan, node) {
		irc_send_cmd(c, "JOIN %s", chan->name);
	}
}

/*
 * return:
 *	a non-negative file descriptor on success
 *	-1 if dns resolution failed
 *	-2 if connection failed
 */
static int irc_net_connect(struct irc_client *c)
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

static int irc_connect(struct irc_client *c)
{
	int fd = irc_net_connect(c);
	if (fd < 0)
		return fd;

	irc_proto_connect(c);

	return fd;
}

int main(int argc, char **argv)
{
	err_set_progname(argv[0]);
	if (argc != 3) {
		fprintf(stderr, "usage: %s <server> <port>\n", argv[0]);
		return -1;
	}

	struct irc_client c = {
		.server = argv[1],
		.port   = argv[2],

		.nick = "bye555",
		.user = "bye555",
		.realname = "bye555",
		.channels = LIST_HEAD_INIT(c.channels)
	};
	irc_join_(&c, "#botwar");
	irc_connect(&c);


	ev_run(EV_DEFAULT_ 0);
	return 0;
}
