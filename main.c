#include <stdio.h>
#include <penny/tcp.h>
#include <penny/print.h>
#include <penny/penny.h>
#include <ccan/container_of/container_of.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdarg.h>

#include <ev.h>

struct conn {
	ev_io w;
	FILE *f;

	/* things the server could potentially change from what I set them to.
	 */
	int mode;
	const char *nick;

	/* We need these to connect, put them here. */
	const char *realname;
	const char *user;
	const char *pass;
};

static void send_irc_cmd(struct conn *c, char const *str, ...)
{

	va_list va;
	va_start(va, str);
	vfprintf(c->f, str, va);
	va_end(va);
	fputs("\r\n", c->f);
	fflush(c->f);
}

static void conn_cb(EV_P_ ev_io *w, int revents)
{
	char buf[2048];
	struct conn *c = container_of(w, typeof(*c), w);
	ssize_t r = read(w->fd, buf, sizeof(buf));
	if (r == 0) {
		fputs("server closed link, exiting...\n", stdout);
		ev_io_stop(EV_A_ w);
		ev_break(EV_A_ EVBREAK_ALL);
		return;
	}

	if (r == -1) {
		fprintf(stderr, "read fail: %s\n", strerror(errno));
		return;
	}

	if (r > 4 && !strncmp("PING", buf, r)) {
		send_irc_cmd(c, "PING");
	}

	printf("> ");
	print_bytes_as_cstring(buf, r, stdout);
	putchar('\n');
}

static void irc_connect(struct conn *c)
{
	if (c->pass)
		send_irc_cmd(c, "PASS %s", c->pass);
	send_irc_cmd(c, "NICK %s", c->nick);
	send_irc_cmd(c, "USER %s %d %s %s", c->user, c->mode, "ignore", c->realname);
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: %s <server> <port>\n", argv[0]);
		return -1;
	}


	struct addrinfo *res;
	int r = tcp_resolve_as_client(argv[1], argv[2], &res);
	if (r) {
		fprintf(stderr, "resolve failure, %s\n", gai_strerror(r));
		return 1;
	}

	int fd = tcp_connect(res);
	if (fd == -1) {
		fprintf(stderr, "connection failed\n");
		return 1;
	}


	struct conn conn;
	conn.f = fdopen(fd, "a");
	conn.nick = "bye555";
	conn.mode = 0;
	conn.user = "bye555";
	conn.realname = "bye555";
	conn.pass = NULL;

	ev_io_init(&conn.w, conn_cb, fd, EV_READ);
	ev_io_start(EV_DEFAULT, &conn.w);

	irc_connect(&conn);

	ev_run(EV_DEFAULT, 0);
	return 0;
}
