#include "mpack.h"

#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>

struct mpack_packer_fd {
	/* must be first */
	struct mpack_packer p;
	int fd;
};

static int mpack_packer_fd_write(mpack_packer *p, void *data, size_t len)
{
	struct mpack_packer_fd *f = (void *)p;
	return write(f->fd, data, len);
}

struct mpack_packer *mpack_packer_fd_create(int fd)
{
	struct mpack_packer_fd *m = malloc(sizeof(*m));
	if (!m)
		return NULL;

	*m = (struct mpack_packer_fd) {
		.fd = fd,
		.p.write = mpack_packer_fd_write,
	};

	return &m->p;
}

void mpack_packer_fd_destroy(struct mpack_packer *p)
{
	free(p);
}
