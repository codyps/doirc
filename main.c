#include "irc.h"

#include <ccan/compiler/compiler.h>
#include <ccan/err/err.h>

#include <penny/mem.h>

#include <stdio.h>

static int on_privmsg(struct irc_connection *c,
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

static int on_join(struct irc_connection *c,
		char const *ch, size_t ch_len)
{
	return 0;
}

static int on_part(struct irc_connection *c,
		char const *ch, size_t ch_len)
{
	return 0;
}

static int on_kick(struct irc_connection *c,
		char const *kicker, size_t kicker_len,
		char const *chan, size_t chan_len,
		char const *nick, size_t nick_len,
		char const *reason, size_t reason_len)
{
	if (irc_user_is_me(c, nick, nick_len)) {
		printf("I was kicked from %.*s because \"%.*s\" rejoin\n",
				(int)chan_len, chan, (int)reason_len, reason);
		irc_cmd_join(c, chan, chan_len);
	}
	return 0;
}

static int on_disconnect(UNUSED struct irc_connection *c)
{
	printf("server booted us.\n");
	return 0;
}

static int on_connect(struct irc_connection *c)
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

		.cb = {
			.connect = on_connect,
			.disconnect = on_disconnect,
			.privmsg = on_privmsg,
			.join = on_join,
			.part = on_part,
			.kick = on_kick,
		},
	};
	irc_connect(&c);

	ev_run(EV_DEFAULT_ 0);
	return 0;
}
