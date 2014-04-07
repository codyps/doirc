
#include "user-track.h"
#include "irc.h"
#include <stdio.h>
#include <ccan/array_size/array_size.h>
#include <ccan/container_of/container_of.h>

#include <penny/mem.h>

struct ut_ch {
	struct irc_operation op;
	struct irc_usertrack_channel *ut;
};

static struct ut_ch *op_to_ut_ch(struct irc_operation *op)
{
	return container_of(op, struct ut_ch, op);
}

/*
 * Space seperated argument handling
 */
static struct arg first_space_arg(const char *start, const char *end)
{
	const char *arg_end = memchr(start, ' ', end - start);
	if (arg_end)
		return (struct arg){ start, arg_end - start };
	else
		return (struct arg){ start, end - start };
}

static struct arg next_space_arg(struct arg a, const char *end)
{
	if (a.data + a.len >= end)
		return (struct arg) {0, 0};
	else
		return first_space_arg(a.data + a.len + 1, end);
}

#define irc_for_each_space_arg(arg, base_arg)		\
	for (arg = first_space_arg(base_arg.data, base_arg.data + base_arg.len);	\
	     arg.len;	\
	     arg = next_space_arg(arg, base_arg.data + base_arg.len))

/*
 * IRC arg parsing (space + ':')
 */
static struct arg first_arg(const char *start, const char *end)
{
	if (start != end && *start == ':')
		return (struct arg){ start + 1, end - (start + 1)};
	return first_space_arg(start, end);
}

static struct arg next_arg(struct arg a, const char *end)
{
	if (a.data + a.len >= end)
		return (struct arg) {0, 0};
	else
		return first_arg(a.data + a.len + 1, end);
}

#define irc_for_each_arg(arg, base_arg)		\
	for (arg = first_arg(base_arg.data, base_arg.data + base_arg.len);	\
	     arg.len;	\
	     arg = next_arg(arg, base_arg.data + base_arg.len))


static bool is_user_op_marker(int c)
{
	switch (c) {
	case '@':
	case '+':
		return true;
	default:
		return false;
	}
}

static uint32_t user_hash_name(const char *n, size_t n_len)
{
	return tommy_hash_u32(0, n, n_len);
}

static uint32_t user_hash(struct irc_user *u)
{
	return user_hash_name(u->nick, u->nick_len);
}

static void add_nick_to_channel(struct irc_usertrack_channel *ut, struct arg nick)
{
	if (!nick.len)
		return;

	printf("ADD %.*s\n", (int)nick.len, nick.data);

	int op = '\0';
	if (is_user_op_marker(*nick.data)) {
		op = *nick.data;
		nick.data ++;
		nick.len --;
	}

	struct irc_user *u = malloc(offsetof(struct irc_user, nick[nick.len]));
	if (!u)
		return;

	u->user_op = op;
	u->nick_len = nick.len;
	memcpy(u->nick, nick.data, nick.len);

	tommy_hashlin_insert(&ut->users, &u->node, u, user_hash(u));
}

static int compare_arg_to_user(const void *arg_, const void *user_)
{
	const struct irc_user *u = user_;
	const struct arg *a = arg_;

	return !memeq(a->data, a->len, u->nick, u->nick_len);
}

static void remove_nick_from_channel(struct irc_usertrack_channel *ut, struct arg nick)
{
	struct irc_user *u = tommy_hashlin_remove(&ut->users, compare_arg_to_user,
			&nick, user_hash_name(nick.data, nick.len));

	if (u) {
		free(u);
	} else {
		printf("COULD NOT FIND USER %.*s to remove\n", (int)nick.len, nick.data);
	}
}

/*
 * RFC 1459:
 *
 *   "<channel> :[[@|+]<nick> [[@|+]<nick> [...]]]"
 *
 * RFC 2812:
 *
 *   ( "=" / "*" / "@" ) <channel> :[ "@" / "+" ] <nick> *( " " [ "@" / "+" ] <nick> )
 *   - "@" is used for secret channels, "*" for private
 *     channels, and "=" for others (public channels).
 *
 * EFNET/FREENODE observed:
 * <my-nick>  ( "=" / "*" / "@" ) <channel> :[ "@" / "+" ] <nick> *( " " [ "@" / "+" ] <nick> )
 */
static int handle_names(struct irc_connection *c,
		struct irc_operation *op,
		const char *prefix, size_t prefix_len,
		const char *remain, size_t remain_len)
{
	struct arg args[2];
	int r = irc_parse_last_args(remain, remain_len, args, ARRAY_SIZE(args));

	if (r != ARRAY_SIZE(args)) {
		printf("arg parse failure: %.*s\n", (int)remain_len, remain);
		return -1;
	}

	struct irc_usertrack_channel *ut = op_to_ut_ch(op)->ut;
	if (!memeq(ut->channel, ut->channel_len, args[0].data, args[0].len))
		return 0;

	struct arg a;
	irc_for_each_space_arg(a, args[1]) {
		add_nick_to_channel(ut, a);
	}
	return 0;
}

/*  */
static int handle_join(struct irc_connection *c,
		struct irc_operation *op,
		const char *prefix, size_t prefix_len,
		const char *remain, size_t remain_len)
{
	struct arg channel = first_arg(remain, remain + remain_len);
	if (!channel.len)
		return -1;

	if (!prefix_len)
		return -1;

	const char *nick_end = memchr(prefix, '!', prefix_len);
	if (!nick_end)
		return -1;

	struct irc_usertrack_channel *ut = op_to_ut_ch(op)->ut;
	if (!memeq(ut->channel, ut->channel_len, channel.data, channel.len))
		return 0;

	add_nick_to_channel(ut, (struct arg){ prefix, nick_end - prefix });
	return 0;
}

static int handle_part(struct irc_connection *c,
		struct irc_operation *op,
		const char *prefix, size_t prefix_len,
		const char *remain, size_t remain_len)
{
	struct arg channel = first_arg(remain, remain + remain_len);
	if (!channel.len)
		return -1;

	if (!prefix_len)
		return -1;

	const char *nick_end = memchr(prefix, '!', prefix_len);
	if (!nick_end)
		return -1;

	struct irc_usertrack_channel *ut = op_to_ut_ch(op)->ut;
	if (!memeq(ut->channel, ut->channel_len, channel.data, channel.len))
		return 0;

	remove_nick_from_channel(ut, (struct arg){ prefix, nick_end - prefix });
	return 0;
}

int irc_add_usertrack_channel(struct irc_connection *c,
		struct irc_usertrack_channel *u)
{
	struct ut_ch *ut_namreply = malloc(sizeof(*ut_namreply));
	if (!ut_namreply)
		return -1;
	struct ut_ch *ut_join = malloc(sizeof(*ut_join));
	if (!ut_join)
		goto e_join;
	struct ut_ch *ut_part = malloc(sizeof(*ut_part));
	if (!ut_part)
		goto e_part;

	*ut_namreply = (struct ut_ch) {
		.op = {
			.type = IRC_OP_NUM,
			.num = RPL_NAMREPLY,
			.cb = handle_names,
		},
		.ut = u,
	};

	*ut_join = (struct ut_ch) {
		.op = IRC_OP_STR_INIT(handle_join, "JOIN"),
		.ut = u,
	};

	*ut_part = (struct ut_ch) {
		.op = IRC_OP_STR_INIT(handle_part, "PART"),
		.ut = u,
	};

	irc_add_operation(c, &ut_namreply->op);
	irc_add_operation(c, &ut_join->op);
	irc_add_operation(c, &ut_part->op);
	return 0;

e_part:
	free(ut_join);
e_join:
	free(ut_namreply);
	return -1;
}

void irc_ut_channel_init_(struct irc_usertrack_channel *ut, const char *channel, size_t channel_len)
{
	*ut = (struct irc_usertrack_channel) {
		.channel = channel,
		.channel_len = channel_len,
	};
	tommy_hashlin_init(&ut->users);
}
