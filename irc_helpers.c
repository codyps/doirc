#include "irc_helpers.h"
#include "irc.h"

#include <ccan/array_size/array_size.h>
#include <ccan/pr_debug/pr_debug.h>

/*
 * PING
 */
static int irc_helper_ping(struct irc_connection *c, struct irc_operation *op,
		const char *prefix, size_t prefix_len,
		const char *remain, size_t remain_len)
{
	/* XXX: ensure @p has a server spec. */
	irc_cmd_fmt(c, "PONG %.*s", (int)remain_len, remain);
	return 0;
}

int irc_add_ping_handler(struct irc_connection *c)
{
	return irc_create_operation_str(c, "PING", irc_helper_ping);
}

/*
 * PRIVMSG
 */
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
		return (struct arg) {NULL, 0};
	else
		return first_comma_arg(a.data + a.len + 1, end);
}

#define irc_for_each_comma_arg(arg, base_arg)		\
	for (arg = first_comma_arg(base_arg.data, base_arg.data + base_arg.len);	\
	     arg.len;	\
	     arg = next_comma_arg(arg, base_arg.data + base_arg.len))

int privmsg_helper(struct irc_connection *c, struct irc_operation *op,
		const char *prefix, size_t prefix_len,
		const char *start, size_t len,
		privmsg_cb cb)
{
	struct arg args[2];
	int r = irc_parse_args(start, len, args, ARRAY_SIZE(args));
	if (r != ARRAY_SIZE(args)) {
		pr_debug(0, "PRIVMSG requires exactly %zu arguments, got %d",
				ARRAY_SIZE(args), r);
		return -1;
	}

	pr_debug(2, "privmsg recipients: ");
	struct arg a;
	size_t dest_ct = 0;
	struct arg *dests = NULL;
	irc_for_each_comma_arg(a, args[0]) {
		pr_debug(2, ":: %.*s ", (int)a.len, a.data);
		/* HAHAHA */
		dests = alloca(sizeof(*dests));
		*dests = a;
		dest_ct++;
	}
	pr_debug(2, "\n");

	if (!dests) {
		pr_debug(0, "PRIVMSG: no destinations, ignoring.");
		return -1;
	}
	/* OH GOD MY SIDES */
	dests -= dest_ct - 1;

	pr_debug(2, "message contents: %.*s\n", (int)args[1].len, args[1].data);

	return cb(c, op, prefix, prefix_len, dests, dest_ct,
			args[1].data, args[1].len);
}

#if 0
static int handle_mode(struct irc_connection *c,
		const char *prefix, size_t prefix_len,
		const char *start, size_t len)
{
	struct arg args[4];
	int r = irc_parse_args(start, len, args, ARRAY_SIZE(args));
	if (r < 1) {
		pr_debug(-1, "MODE: could not parse args: %d", r);
		return -1;
	}

	if (c->cb.mode)
		c->cb.mode(c, prefix, prefix_len,
				args, r);
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
			(int)args[0].len, args[0].data,
			(int)args[1].len, args[1].data,
			(int)args[2].len, args[2].data);
	return 0;
}
#endif
