#include "irc.h"
#include "irc_helpers.h"

#include <ccan/compiler/compiler.h>
#include <ccan/err/err.h>
#include <ccan/array_size/array_size.h>
#include <ccan/pr_debug/pr_debug.h>

#include <penny/mem.h>

#include <stdio.h>

static int do_privmsg(struct irc_connection *c, struct irc_operation *op,
		char const *src, size_t src_len,
		struct arg *dests, size_t dest_ct,
		char const *msg, size_t msg_len)
{
	const char *name_end = memchr(src, '!', src_len);
	if (memeqstr(msg, msg_len, ",hi")) {
		irc_cmd_privmsg_fmt(c, dests[0].data, dests[0].len,
				"HI, %.*s", (int)(name_end - src), src);
	}
	return 0;
}

static int on_privmsg(struct irc_connection *c, struct irc_operation *op,
		char const *prefix, size_t prefix_len,
		char const *remain, size_t remain_len)
{
	return privmsg_helper(c, op, prefix, prefix_len, remain,
			remain_len, do_privmsg);
}

static int on_kick(struct irc_connection *c, struct irc_operation *op,
		char const *prefix, size_t prefix_len,
		char const *remain, size_t remain_len)
{
	struct arg args[3];
	int r = irc_parse_args(remain, remain_len, args, ARRAY_SIZE(args));
	if (r != ARRAY_SIZE(args)) {
		pr_debug(-1, "KICK: could not parse args: %d", r);
		return -1;
	}

	char const *chan = args[0].data;
	size_t chan_len = args[0].len;
	char const *nick = args[1].data;
	size_t nick_len = args[1].len;
	char const *reason = args[2].data;
	size_t reason_len = args[2].len;



	if (irc_user_is_me(c, nick, nick_len)) {
		printf("I was kicked from %.*s because \"%.*s\" rejoin\n",
				(int)chan_len, chan, (int)reason_len, reason);
		irc_cmd_join(c, chan, chan_len);
	}
	return 0;
}

static int on_connect(struct irc_connection *c, struct irc_operation *op,
		const char *prefix, size_t prefix_len,
		const char *remain, size_t remain_len)
{
	irc_cmd_join_(c, "#botwar");
	return 0;
}

int main(int argc, char **argv)
{
	err_set_progname(argv[0]);
	if (argc != 3) {
		fprintf(stderr, "usage: %s <server> <port>\n", argv[0]);
		return -1;
	}

	struct irc_connection c = {
		.server = argv[1],
		.port   = argv[2],

		.nick = "bye555",
		.user = "bye555",
		.realname = "bye555",
	};

	irc_init_cb(&c);

	DEFINE_IRC_OP_NUM(connect, RPL_WELCOME);
	irc_add_operation(&c, &op_connect);

	DEFINE_IRC_OP_STR(privmsg, "PRIVMSG");
	irc_add_operation(&c, &op_privmsg);

	DEFINE_IRC_OP_STR(kick, "KICK");
	irc_add_operation(&c, &op_kick);

	irc_add_ping_handler(&c);

	irc_connect(&c);

	ev_run(EV_DEFAULT_ 0);
	return 0;
}
