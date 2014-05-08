#ifndef MPACK_H_
#define MPACK_H_
/* A minimal message pack encoder */

#include <stdint.h>

/*** encoder ***/
typedef struct mpack_packer {
	int (write*)(struct mpack_packer *p, void *data, size_t len);
} mpack_packer;

int mpack_pack_uint(mpack_packer *p, uintmax_t i);
int mpack_pack_int(mpack_packer *p, intmax_t i);
int mpack_pack_bytes(mpack_packer *p, void *data, size_t len);
int mpack_pack_array_start(mpack_packer *p, uint_least32_t count);

/** specific encoders **/
/* -> to a file descriptor, mpack_fd.c */
struct mpack_packer *mpack_packer_fd_create(int fd);
void mpack_packer_fd_destroy(struct mpack_packer *p);

/*** decoder ***/
enum mpack_type {
	MPACK_TYPE_UINT,
	MPACK_TYPE_INT,
	MPACK_TYPE_BYTES,
	MPACK_TYPE_ARRAY,
};

struct mpack_obj {
	int ref;
	enum mpack_type type;
	union {
		uintmax_t u;
		intmax_t i;
		struct mpack_bytes {
			uint8_t *data;
			size_t len;
		} bytes;
		size_t array_elems;
	} val;
};

void mpack_obj_up(struct mpack_obj *o);
void mpack_obj_down(struct mpack_obj *o);

typedef struct mpack_decoder {
	int (read *)(struct mpack_decoder *p, void *data, size_t len);
} mpack_decoder;

/*
 * returns a mpack_obj with a single reference count, or NULL on error and EOF
 */
struct mpack_obj *mpack_decoder_next(struct mpack_decoder *p);

/** specific decoders **/
/* from a file descriptor, mpack_decode_fd.c */
struct mpack_decoder *mpack_decoder_fd_create(int fd);
void mpack_decoder_fd_destroy(struct mpack_decoder *p);

#endif
