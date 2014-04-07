#ifndef USER_TRACK_H_
#define USER_TRACK_H_

#include <tommyds/tommyhashlin.h>
#include <string.h>
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

/*
 * @hl is a (tommy_hashlin *)
 * @bucket is a (tommy_node *), cursor, advanced over loop
 * @pos is a (size_t), temporary to track out position
 */
#define tommy_hashlin_for_each_bucket(hl__, bucket__, pos__)			\
		for ((pos__) = 0, (bucket__) = find_bucket((hl__), &(pos__));	\
		     (bucket__);						\
		     (bucket__) = find_bucket((hl__), &(pos__)))

/*
 * @bucket is a (tommy_node *), must not be NULL.
 * @entry  is the object that was inserted into the table, cursor, advanced
 *         over the loop.
 * @next   is a (tommy_node *), temporary to track out position.
 */
#define tommy_node_for_each(bucket_, entry_, next_)		\
		for ((entry_) = (bucket_)->data, (next_) = (bucket_)->next;	\
		     (entry_);							\
		     (entry_) = (next_)->data, (next_) = (next_)->next)

struct hashlin_for_each_temp {
	size_t pos;
	tommy_node *next;
	tommy_node *bucket;
};

/*
 * @hl_ is a (tommy_hashlin *)
 * @obj_ is the object that was inserted into the table, cursor, advanced (void *)
 * @tmp_ is a (struct hashlin_for_each_temp)
 */
#define tommy_hashlin_for_each(hl_, obj_, tmp_)				\
	tommy_hashlin_for_each_bucket((hl_), (tmp_).bucket, (tmp_).pos)		\
	tommy_node_for_each((tmp_).bucket, obj_, (tmp_).next)


#define irc_usertrack_channel_for_each_user(utc_, user_, tmp_)	\
	tommy_hashlin_for_each(&(utc_)->users, user_, tmp_)

#endif
