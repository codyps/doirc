#include "mpack.h"

#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>

struct mpack_decoder_fd {
	/* must be first */
	struct mpack_decoder p;
	int fd;
};

static int mpack_decoder_fd_read(mpack_decoder *p, void *data, size_t len)
{
	struct mpack_decoder_fd *f = (void *)p;
	return read(f->fd, data, len);
}

struct mpack_decoder *mpack_decoder_fd_create(int fd)
{
	struct mpack_decoder_fd *m = malloc(sizeof(*m));
	if (!m)
		return NULL;

	*m = (struct mpack_decoder_fd) {
		.fd = fd,
		.p.read = mpack_decoder_fd_read,
	};

	return &m->p;
}

void mpack_decoder_fd_destroy(struct mpack_decoder *p)
{
	free(p);
}
