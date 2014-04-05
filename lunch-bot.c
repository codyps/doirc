#include "irc.h"

#include <ccan/compiler/compiler.h>
#include <ccan/err/err.h>
#include <ccan/array_size/array_size.h>

#include <penny/mem.h>

#include <stdio.h>

struct msg_source {
	enum {
		MS_PRIV,
		MS_CHAN,
	} src;

	/* Either a channel name (with leading chars) or NULL if none was
	 * provided */
	const char *channel;
	size_t channel_len;

	/* The sending user (always populated) */
	const char *user;
	size_t user_len;
};

static int PRINTF_FMT(3,4) msg_reply_fmt(struct irc_connection *c, const struct msg_source *src, const char *fmt, ...)
{
	va_list va;
	const char *d;
	size_t d_len;
	if (src->src == MS_PRIV) {
		d = src->user;
		d_len = src->user_len;
	} else if (src->src == MS_CHAN) {
		d = src->channel;
		d_len = src->channel_len;
	}

	va_start(va, fmt);
	int r = irc_cmd_privmsg_va(c, d, d_len, fmt, va);
	va_end(va);
	return r;
}

typedef int (*command_callback_t)(struct irc_connection *c, const struct msg_source *src, const char *cmd, size_t cmd_len, const char *arg, size_t arg_len);

struct command {
	const char *cmd;
	size_t cmd_len;
	command_callback_t cb;
};

#define CMD_(name, func) { .cmd = #name, .cmd_len = sizeof(#name) - 1, .cb = func }
#define CMD(name) CMD_(name, cmd_##name)

int cmd_magic = '.';

#define OWNER(n) { .name = n, .name_len = sizeof(n) - 1 }
static struct owner {
	const char *name;
	size_t name_len;
} owner = OWNER("x1");

static void msg_owner(struct irc_connection *c, char const *fmt, ...)
{
	va_list va, va2;
	va_start(va, fmt);
	va_copy(va2, va);
	irc_cmd_privmsg_va(c, owner.name, owner.name_len, fmt, va);
	vprintf(fmt, va2);
	va_end(va2);
	va_end(va);
}

static int cmd_unknown(struct irc_connection *c, const struct msg_source *src, const char *cmd, size_t cmd_len, const char *msg, size_t msg_len)
{
	msg_reply_fmt(c, src, "I don't know the command \"%.*s\", try `%chelp`", (int)cmd_len, cmd, cmd_magic);
	return 0;
}

static int cmd_help(struct irc_connection *c, const struct msg_source *src, const char *cmd, size_t cmd_len, const char *msg, size_t msg_len)
{
	irc_cmd_privmsg_fmt(c, src->user, src->user_len, "HI\n");
	return 0;
}

struct command commands [] = {
	CMD(unknown), /* this is triggered when the command isn't recognized */
	CMD(help),
};

static void run_command(struct irc_connection *c, struct msg_source *src,
		char const *msg, size_t msg_len)
{
	size_t i;
	const char *cmd_start = msg + 1;
	const char *cmd_end = memchr(cmd_start, ' ', msg_len - 1);
	size_t cmd_len = cmd_end ? cmd_end - cmd_start : msg_len - 1;
	const char *arg;
	size_t arg_len;
	if (!cmd_end) {
		arg_len = 0;
		arg = NULL;
	} else {
		arg = cmd_end + 1;
		arg_len = msg_len - cmd_len - 2;
	}
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (memeq(cmd_start, cmd_len, commands[i].cmd, commands[i].cmd_len)) {
			commands[i].cb(c, src, cmd_start, cmd_len, arg, arg_len);
			return;
		}
	}

	commands[0].cb(c, src, cmd_start, cmd_len, arg, arg_len);
}

static int on_privmsg(struct irc_connection *c,
		char const *src, size_t src_len,
		struct arg *dests, size_t dest_ct,
		char const *msg, size_t msg_len)
{
	const char *name_end = memchr(src, '!', src_len);
	printf("PRIV: %.*s %.*s (ct=%d) %.*s\n", (int)src_len, src, (int) dests[0].len, dests[0].data, (int)dest_ct, (int)msg_len, msg);

	if (dest_ct < 1) {
		msg_owner(c, "weird dest_ct=%d\n", dest_ct);
	}

	if (dests[0].len < 1) {
		msg_owner(c, "dest length < 1\n");
		return 0;
	}

	struct msg_source msg_src = {
		.user = src,
		.user_len = name_end - src,
	};

	if (*dests[0].data == '#') {
		msg_src.channel = dests[0].data;
		msg_src.channel_len = dests[0].len;
		msg_src.src = MS_CHAN;
	} else {
		msg_src.src = MS_PRIV;
		/* TODO: identify the channel by partially parsing the command. */
	}

	if (msg_len > 0 && *msg == cmd_magic)
		run_command(c, &msg_src, msg, msg_len);

	/* TODO: also check if someone is calling us by name */
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
	irc_cmd_join_(c, "#test-lunch-bot");
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

		.nick = "lunch-bot",
		.user = "lunch-bot",
		.realname = "lunch-bot",

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
