struct irc_session {
	struct irc_connection;


	struct list_head channels;
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

static struct irc_channel *get_channel_by_name(struct irc_connection *c,
		const char *name, size_t len)
{
	struct irc_channel *ch;
	list_for_each(&c->channels, ch, node) {
		if (memeq(name, len, ch->name, ch->name_len))
			return ch;
	}

	return NULL;
}
