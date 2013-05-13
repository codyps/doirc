#include "irc.h"

#include <ccan/compiler/compiler.h>
#include <ccan/err/err.h>

#include <stdio.h>

static int on_privmsg(struct irc_connection *c,
		char const *src, size_t src_len,
		char const *dest, size_t dest_len,
		char const *msg, size_t msg_len)
{
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
		},
	};
	irc_connect(&c);


	ev_run(EV_DEFAULT_ 0);
	return 0;
}
