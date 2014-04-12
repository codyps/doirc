#include "irc.h"
#include "irc_helpers.h"
#include "user-track.h"

#include <ccan/pr_debug/pr_debug.h>
#include <ccan/compiler/compiler.h>
#include <ccan/err/err.h>
#include <ccan/array_size/array_size.h>
#include <ccan/container_of/container_of.h>

#include <penny/mem.h>

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>

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

struct irc_ctx {
	struct irc_connection c;
	struct irc_usertrack_channel ut;
	const char *prgm;
};

static struct irc_ctx *con_to_ctx(struct irc_connection *c)
{
	return container_of(c, struct irc_ctx, c);
}

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

#define FMT_TOMMY_NODE "{ .data = %p, .key = %#llx, .next = %p, .prev = %p }"
#define EXP_TOMMY_NODE(n_) (n_).data, (unsigned long long)(n_).key, (n_).next, (n_).prev

#define EXP_TOMMY_HASHLIN(hl_)	(hl_).bucket_bit, (hl_).bucket_max,	\
		(hl_).bucket_mask, (hl_).bucket_mac, (hl_).low_max,	\
		(hl_).low_mask, (hl_).split, (hl_).state, (hl_).count
#define FMT_TOMMY_HASHLIN "{ .bucket_bit=0x%x, .bucket_max=%u, .bucket_mask=0x%x, .bucket_mac=%u, .low_max=%u, .low_mask=0x%x, .split=%u, .state=%u, .count=%u }"

static int cmd_ping(struct irc_connection *c, const struct msg_source *src,
		const char *cmd, size_t cmd_len, const char *msg, size_t msg_len)
{
	struct irc_usertrack_channel *ut = &con_to_ctx(c)->ut;
	struct irc_user *u;
	tommy_node *bucket, *node;
	size_t pos;

	printf("|||| "FMT_TOMMY_HASHLIN"\n", EXP_TOMMY_HASHLIN(ut->users));

	irc_usertrack_channel_for_each_user(ut, u, node, bucket, pos) {
		printf("|| %.*s\n", (int)u->nick_len, u->nick);
	}

	return 0;
}

static int cmd_exec(struct irc_connection *c, const struct msg_source *src,
		const char *cmd, size_t cmd_len, const char *msg, size_t msg_len)
{
	const char *prgm = con_to_ctx(c)->prgm;
	char buf[16];
	sprintf(buf, "%u", c->w.fd);
	execlp(prgm, "-f", c->w.fd, c->server, c->port, NULL);
	return 0;
}

struct command commands [] = {
	CMD(unknown), /* this is triggered when the command isn't recognized */
	CMD(help),
	CMD(ping),
	CMD(exec),
};

static void run_command(struct irc_connection *c, struct msg_source *src,
		char const *cmdmsg, size_t cmdmsg_len)
{
	size_t i;
	const char *cmd_start = cmdmsg;
	const char *cmd_end = memchr(cmd_start, ' ', cmdmsg_len - 1);
	size_t cmd_len = cmd_end ? cmd_end - cmd_start : cmdmsg_len - 1;
	const char *arg;
	size_t arg_len;
	if (!cmd_end) {
		arg_len = 0;
		arg = NULL;
	} else {
		arg = cmd_end + 1;
		arg_len = cmdmsg_len - cmd_len - 2;
	}
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (memeq(cmd_start, cmd_len, commands[i].cmd, commands[i].cmd_len)) {
			commands[i].cb(c, src, cmd_start, cmd_len, arg, arg_len);
			return;
		}
	}

	commands[0].cb(c, src, cmd_start, cmd_len, arg, arg_len);
}

/*
 *
 * <cmd_magic> <command>
 * <nick> <non-alnum>* <command>
 *
 * --
 * <command> must begin with a non-punct character
 *
 */
static int do_privmsg(struct irc_connection *c, struct irc_operation *op,
		char const *src, size_t src_len,
		struct arg *dests, size_t dest_ct,
		char const *msg, size_t msg_len)
{
	const char *name_end = memchr(src, '!', src_len);
	printf("PRIV: %.*s %.*s (ct=%d) %.*s\n", (int)src_len, src,
			(int) dests[0].len, dests[0].data,
			(int)dest_ct, (int)msg_len, msg);

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

	/* check if someone is calling us by name */
	if (memstarts(msg, msg_len, c->nick, c->nick_len) &&
			(msg_len - c->nick_len) > 0 && !isalnum(*(msg + c->nick_len))) {
		const char *cmd_start = msg + c->nick_len + 1;
		size_t remain_len = msg_len - c->nick_len - 1;
		/* scan until we get a non-punc char */
		while (remain_len && !ispunct(*cmd_start)) {
			cmd_start ++;
			remain_len --;
		}

		run_command(c, &msg_src, cmd_start, remain_len);
	}
	if (msg_len > 0 && *msg == cmd_magic)
		run_command(c, &msg_src, msg + 1, msg_len - 1);

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

static const char *channel = "#test-lunch-bot";

static int on_connect(struct irc_connection *c, struct irc_operation *op,
		char const *prefix, size_t prefix_len,
		char const *remain, size_t remain_len)
{
	irc_cmd_join_(c, channel);
	return 0;
}

int main(int argc, char **argv)
{
	err_set_progname(argv[0]);
	if (argc != 3) {
		fprintf(stderr, "usage: %s <server> <port>\n", argv[0]);
		return -1;
	}

	struct irc_ctx c = {
		.c = {
			.server = argv[1],
			.port   = argv[2],

			SLM(nick, "lunch-bot"),

			.user = "lunch-bot",
			.realname = "lunch-bot",
		},
		.prgm = argv[0],
	};

	irc_init(&c.c);

	DEFINE_IRC_OP_NUM(connect, RPL_WELCOME);
	irc_add_operation(&c.c, &op_connect);

	DEFINE_IRC_OP_STR(privmsg, "PRIVMSG");
	irc_add_operation(&c.c, &op_privmsg);

	DEFINE_IRC_OP_STR(kick, "KICK");
	irc_add_operation(&c.c, &op_kick);

	irc_ut_channel_init(&c.ut, channel);
	irc_add_usertrack_channel(&c.c, &c.ut);

	irc_add_ping_handler(&c.c);

	irc_connect(&c.c);

	ev_run(EV_DEFAULT_ 0);
	return 0;
}
