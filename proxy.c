/*
 * ## IRC specific pieces
 * - assumes a line-based protocol
 * - special handling of privmsg (any others?)
 * - user <-> proxy
 *   - user negotiation
 * - proxy <-> server
 *   - NickServ auth handling
 *
 *
 * ## Design
 * 1 "user" -> a password to auth with the proxy
 * Many "servers"
 *  - each can be plain or ssl
 * Many "clients"
 *  - each selects a particular server
 */

/*
 * USER <user>[#<server>[#<client>]]
 *
 * 'servers' must be explicitly configured by chatting with the special user '*'
 *
 * XXX: what about initial startup?
 *  - Initial user must be created... somehow
 *    - config file?
 *    - unix socket interface?
 *  - USER <user>
 *
 * - config + data storage dir
 *   /<user>/password
 *   /<user>/clients/<client>/position
 *   /<user>/servers/<server>/hostname
 *   /<user>/servers/<server>/ssl
 *
 *   /<user>/logs/<log-files>
 *   OR??
 *   /<user>/servers/<server>/logs/<log-files>
 */

struct server;
struct user;

struct client {
	const char *id;
	struct rb_node node;
	struct server *server;
	tommy_node node_from_server;
	tommy_node node_from_proxy;

	/* position to start reading from */
};

struct server {
	const char *name; /* unique */
	const char *host; /* hostname '/' port */
	bool ssl;
	tommy_hashlin clients;
	tommy_node node_from_user;
	struct user *user;
};

struct user {
	const char *name;
	tommy_hashlin servers; /* struct server -> node_from_user */
	tommy_node node_from_proxy;
};

struct proxy {
	tommy_hashlin users; /* struct user -> node_from_proxy */
	/* directory to read data from and store data to */
	DIR *dir;

	/* How do we handle filesystem modifications happening in parallel to our lookups?
	 * Should we just use the FS as our "data structures"? */
	tommy_hashlin clients; /* struct client -> node_from_proxy */
};

int main(int argc, char *argv[])
{
	return 0;
}
