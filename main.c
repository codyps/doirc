#include <stdio.h>
#include <penny/tcp.h>
#include <penny/print.h>
#include <penny/penny.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdarg.h>

#include <ev.h>

struct conn {
	ev_io w;
	FILE *f;
};

static void conn_cb(EV_P_ ev_io *w, int revents)
{
	char buf[2048];
	ssize_t r = read(w->fd, buf, sizeof(buf));
	if (r == 0 && errno != EAGAIN) {
		fprintf(stderr, "read fail: %s\n", strerror(errno));
		ev_io_stop(EV_A_ w);
		ev_break(EV_A_ EVBREAK_ALL);
	}

	if (r == -1) {
		fprintf(stderr, "read fail: %s\n", strerror(errno));
		return;
	}
	printf("> ");
	print_bytes_as_cstring(buf, r, stdout);
	putchar('\n');
}

static void send_irc_cmd(struct conn *c, char const *str, ...)
{

	va_list va;
	va_start(va, str);
	vfprintf(c->f, str, va);
	va_end(va);
	fputs("\r\n", c->f);
}

static void irc_connect(struct conn *c, char const *pass,
		char const *nick, char const *user)
{
	if (pass)
		send_irc_cmd(c, "PASS %s", pass);
	if (nick)
		send_irc_cmd(c, "NICK %s", nick);
	if (user)
		send_irc_cmd(c, "USER %s", user);
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
	ev_io_init(&conn.w, conn_cb, fd, EV_READ);
	ev_io_start(EV_DEFAULT, &conn.w);

	irc_connect(&conn, "hi", "bye_555", "sigh");

	ev_run(EV_DEFAULT, 0);
	return 0;
}
