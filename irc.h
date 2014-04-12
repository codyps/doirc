#ifndef IRC_H_
#define IRC_H_

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

#include <ccan/compiler/compiler.h>
#include <tommyds/tommyhashlin.h>

#include <ev.h>

enum irc_num_cmds {
#define RPL(name, value) RPL_##name = value,
#include "irc_spec.h"
#undef RPL
};

static const char * const irc_num_cmds[] = {
#define RPL(name, value) [value] = #name,
#include "irc_spec.h"
#undef RPL
};

enum irc_user_mode {
	IRC_UM_i = 1 << 0,
	IRC_UM_w = 1 << 1,
	IRC_UM_s = 1 << 2,
	IRC_UM_o = 1 << 3,
};

enum irc_channel_user_mode {
	IRC_CUM_v = 1 << 0,
	IRC_CUM_o = 1 << 1,
};

enum irc_channel_mode {
	IRC_CM_s = 1 << 0,
};

struct arg {
	const char *data;
	size_t len;
};

struct irc_connection;
struct irc_operation;

typedef int (*irc_op_cb)(struct irc_connection *c, struct irc_operation *op,
		const char *prefix, size_t prefix_len,
		const char *start, size_t len);

struct irc_operation {
	tommy_node node;
	enum {
		IRC_OP_STR,
		IRC_OP_NUM,
	} type;
	union {
		struct {
			const char *str;
			size_t str_len;
		};
		unsigned num;
	};
	irc_op_cb cb;
	/* XXX: we probably need a destructor */
};

#define SLM(id, str) .id = str, .id##_len = strlen(str)
struct irc_connection {
	ev_io w;

	/* network connection */
	const char *server;
	const char *port;

	/* irc proto connection */
	const char *nick;
	const char *realname;
	const char *user;
	const char *pass;

	size_t nick_len;

	/* (struct irc_operation *) */
	tommy_hashlin operations;

#if 0
	/* state while connected */
	enum irc_user_mode user_mode;
	struct list_head channels;
#endif

	/* buffers */
	size_t in_pos;
	char in_buf[1024];
};

/* for use in callbacks */
int irc_cmd(struct irc_connection *c,
		char const *msg, size_t msg_len);
int irc_cmd_privmsg(struct irc_connection *c,
		char const *dest, size_t dest_len,
		char const *msg,  size_t msg_len);

int PRINTF_FMT(2,3) irc_cmd_fmt(struct irc_connection *c,
		char const *str, ...);
int PRINTF_FMT(4,5) irc_cmd_privmsg_fmt(struct irc_connection *c,
		char const *dest, size_t dest_len,
		char const *msg_fmt, ...);
int irc_cmd_privmsg_va(struct irc_connection *c,
		char const *dest, size_t dest_len,
		char const *msg_fmt, va_list va);

#define irc_cmd_join_(c, n) irc_cmd_join(c, n, strlen(n))
int irc_cmd_join(struct irc_connection *c,
		char const *name, size_t name_len);
int irc_cmd_invite(struct irc_connection *c,
		char const *nick, size_t nick_len,
		char const *chan, size_t chan_len);

bool irc_user_is_me(struct irc_connection *c, const char *start, size_t len);

int irc_clear_channel_user_mode(struct irc_connection *c,
		const char *channel, size_t channel_len,
		const char *name, size_t name_len,
		enum irc_channel_user_mode mode);
int irc_set_channel_user_mode(struct irc_connection *c,
		const char *channel, size_t channel_len,
		const char *name, size_t name_len,
		enum irc_channel_user_mode mode);


/*
 * callback managment
 */
int irc_create_operation_num(struct irc_connection *c,
		unsigned num, irc_op_cb cb);

/* str is assumed to continue to exist until the op is removed */
int irc_create_operation_str_(struct irc_connection *c,
		const char *str, size_t str_len, irc_op_cb cb);
static inline int irc_create_operation_str(struct irc_connection *c,
		const char *str, irc_op_cb cb)
{
	return irc_create_operation_str_(c, str, strlen(str), cb);
}

/* op is assumed to continue to exist until the op is removed */
void irc_add_operation(struct irc_connection *c, struct irc_operation *op);

#define IRC_OP_STR_INIT(cb_, str_) {	\
	.type = IRC_OP_STR,		\
	.str = str_,			\
	.str_len = sizeof(str_) - 1,	\
	.cb = cb_,			\
}

#define DEFINE_IRC_OP_STR(name_, str_)		\
	struct irc_operation op_##name_ = IRC_OP_STR_INIT(on_##name_, str_)

#define DEFINE_IRC_OP_NUM(name_, num_)		\
	struct irc_operation op_##name_ = {		\
		.type = IRC_OP_NUM,		\
		.num = num_,			\
		.cb = on_##name_,		\
	}

/* must be called before callbacks are added */
void irc_init(struct irc_connection *c);

/*
 * utility
 */
int irc_parse_args(char const *start, size_t len, struct arg *args,
		size_t max_args);

/* instead of getting the first <n> arguments, get the last <n> */
int irc_parse_last_args(char const *start, size_t len, struct arg *args,
		size_t max_args);
/*
 * connection managment
 */
int irc_connect(struct irc_connection *c);
void irc_connect_fd(struct irc_connection *c, int fd);
void irc_disconnect(struct irc_connection *c);
bool irc_is_connected(struct irc_connection *c);


#endif
