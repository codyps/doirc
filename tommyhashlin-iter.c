#include <tommyds/tommyhashlin.h>

#include <penny/mem.h>
#include <penny/compiler.h>

#include <ccan/breakpoint/breakpoint.h>

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

struct irc_user {
	tommy_node node;
	size_t nick_len;
	char nick[];
};

struct irc_usertrack_channel {
	tommy_hashlin users;
};

struct arg {
	const char *data;
	size_t len;
};

static uint32_t user_hash_name(const char *n, size_t n_len)
{
	return tommy_hash_u32(0, n, n_len);
}

static uint32_t user_hash(struct irc_user *u)
{
	return user_hash_name(u->nick, u->nick_len);
}

static int compare_arg_to_user(const void *arg_, const void *user_)
{
	const struct irc_user *u = user_;
	const struct arg *a = arg_;

	return !memeq(a->data, a->len, u->nick, u->nick_len);
}

static void add_nick_to_channel(struct irc_usertrack_channel *ut, struct arg nick)
{
	if (!nick.len)
		return;

	struct irc_user *u = tommy_hashlin_search(&ut->users, compare_arg_to_user, &nick,
				user_hash_name(nick.data, nick.len));

	if (u)
		return;

	printf("ADD %.*s\n", (int)nick.len, nick.data);

	u = malloc(offsetof(struct irc_user, nick[nick.len]));
	if (!u)
		return;

	u->nick_len = nick.len;
	memcpy(u->nick, nick.data, nick.len);

	tommy_hashlin_insert(&ut->users, &u->node, u, user_hash(u));
}

static struct irc_usertrack_channel c;

static void add(const char *n)
{
	struct arg a = {
		.data = (char *)n,
		.len = strlen(n),
	};

	add_nick_to_channel(&c, a);
}

int main(int argc, char **argv)
{
	tommy_hashlin_init(&c.users);

	add("foo");
	add("bar");
	add("bad");

	tommy_hashlin *hl = &c.users;
	unsigned i, j;
	tommy_node *n;
	struct irc_user *u;
	tommy_hashlin_for_each_entry(hl, u, n, i, j) {
		printf("GOT: %.*s\n", (int)u->nick_len, u->nick);
	}

	breakpoint();

	return 0;
}
