#ifndef IRC_HELPERS_H_
#define IRC_HELPERS_H_

#include <stdlib.h>

struct irc_connection;
struct irc_operation;
struct arg;

int irc_add_ping_handler(struct irc_connection *c);


typedef int (*privmsg_cb)(struct irc_connection *c, struct irc_operation *op,
			char const *source, size_t source_len,
			struct arg *dests, size_t dest_ct,
			char const *msg, size_t msg_len);
int privmsg_helper(struct irc_connection *c, struct irc_operation *op,
		const char *prefix, size_t prefix_len,
		const char *start, size_t len,
		privmsg_cb cb);
#endif
