#ifndef USER_TRACK_H_
#define USER_TRACK_H_

#include <tommyds/tommyhashlin.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

struct irc_user {
	tommy_node node;
	int user_op;
	size_t nick_len;
	char nick[];
};

struct irc_usertrack_channel {
	const char *channel;
	size_t channel_len;

	tommy_hashlin users;
};

/*
struct irc_usertrack {
};
*/

struct irc_connection;


void irc_ut_channel_init_(struct irc_usertrack_channel *ut,
		const char *channel, size_t channel_len);
static inline void irc_ut_channel_init(struct irc_usertrack_channel *ut, const char *channel)
{
	irc_ut_channel_init_(ut, channel, strlen(channel));
}

int irc_add_usertrack_channel(struct irc_connection *c,
		struct irc_usertrack_channel *u);



/* HASHLIN iteration */
static inline void *find_bucket(tommy_hashlin *hl, size_t *pos)
{
	unsigned i;
	for (i = *pos; i < hl->bucket_mac; ++i)
		if (hl->bucket[i]) {
			*pos = i + 1;
			return hl->bucket[i];
		}

	return NULL;
}

static inline void *tommy_hashlin_pos_safe(tommy_hashlin *hl, size_t pos)
{
	if (pos > hl->count)
		return NULL;
	tommy_node **node_ = tommy_hashlin_pos(hl, pos);
	assert(node_);
	tommy_node *node = *node_;
	if (!node)
		return NULL;

	return node;
}

/*
 * @hl__     : a (tommy_hashlin *)
 * @bucket__ : a (tommy_node *)
 * @pos__    : a (size_t) which is set to the current "position" of the obj__ in
 *	       the hash table.
 */
#define tommy_hashlin_for_each_bucket(hl__, bucket__, pos__)			\
	for ((pos__) = 0, (bucket__) = tommy_hashlin_pos_safe((hl__), (pos__));	\
	     (bucket__);								\
	     (pos__)++,   (bucket__) = tommy_hashlin_pos_safe((hl__), (pos__)))

/*
 * tommy_node *list__;
 * tommy_node *node__;
 */
#define tommy_list_for_each(list__, node__)	\
	for ((node__) = (list__);		\
	     (node__);				\
	     (node__) = (node__)->next)

/*
 * XXX: break skips a bucket, it doesn't break out of the entire loop as
 *      expected. Use 'goto' to escape the loop.
 *
 * tommy_hashlin *hl_;
 * tommy_node *node_;   // cursor
 * tommy_node *bucket_;
 * size_t pos_;
 */
#define tommy_hashlin_for_each(hl_, node_, bucket_, pos_)		\
	tommy_hashlin_for_each_bucket((hl_), (bucket_), (pos_))		\
	tommy_list_for_each((bucket_), (node_))

#define irc_usertrack_channel_for_each_user(utc_, user_, node_, bucket_, pos_)	\
	tommy_hashlin_for_each(&(utc_)->users, node_, bucket_, pos_)		\
	for ((user_) = (node_)->data; (user_); (user_) = NULL)

#endif
