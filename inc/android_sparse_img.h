// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_ANDROID_SPARSE_IMG_H
#define _SMOLUTILS_ANDROID_SPARSE_IMG_H

/* See: https://android.googlesource.com/platform/system/core/+/HEAD/libsparse */

#include "io.h"
#include "printing.h"
#include "unaligned.h"

/*
 * An Android sparse image is a header then chunks, little endian.
 * A chunk is a header and a body and covers some blocks of the output.
 */
#define ANDROID_SPARSE_MAGIC		0xed26ff3a
#define ANDROID_SPARSE_HEADER_SZ	28
#define ANDROID_SPARSE_CHUNK_HEADER_SZ	12

#define ANDROID_SPARSE_CHUNK_RAW	0xcac1
#define ANDROID_SPARSE_CHUNK_FILL	0xcac2
#define ANDROID_SPARSE_CHUNK_DONT_CARE	0xcac3
#define ANDROID_SPARSE_CHUNK_CRC32	0xcac4

struct android_sparse_img {
	unsigned long long size;
	unsigned int chunk_hdr_sz;
	uint32_t blk_sz;
	uint32_t chunks;
	int fd;
};

struct android_sparse_chunk {
	unsigned long long len;	/* bytes of output it covers */
	unsigned int type;
	uint8_t fill[4];
};

static inline bool android_sparse_is(const uint8_t *hdr)
{
	return get_unaligned_le32(hdr) == ANDROID_SPARSE_MAGIC;
}

/* Check header and park at first chunk */
static inline int android_sparse_open(struct android_sparse_img *img, int fd,
				      const uint8_t *hdr)
{
	unsigned int hdr_sz = get_unaligned_le16(hdr + 8);

	img->fd = fd;
	img->chunk_hdr_sz = get_unaligned_le16(hdr + 10);
	img->blk_sz = get_unaligned_le32(hdr + 12);
	img->chunks = get_unaligned_le32(hdr + 20);
	img->size = (unsigned long long) get_unaligned_le32(hdr + 16) *
		    img->blk_sz;

	if (get_unaligned_le16(hdr + 4) != 1 ||
	    hdr_sz < ANDROID_SPARSE_HEADER_SZ ||
	    img->chunk_hdr_sz < ANDROID_SPARSE_CHUNK_HEADER_SZ ||
	    !img->blk_sz || img->blk_sz % 4) {
		error("Unsupported sparse image\n");
		return -1;
	}

	if (lseek(fd, hdr_sz, SEEK_SET) != (off_t) hdr_sz) {
		error("Failed to seek in the image: %d\n", errno);
		return -1;
	}

	return 0;
}

static inline int android_sparse_chunk(const struct android_sparse_img *img,
				       uint32_t i,
				       struct android_sparse_chunk *c)
{
	uint8_t hdr[ANDROID_SPARSE_CHUNK_HEADER_SZ];
	unsigned long long body;
	unsigned int total;

	if (read_full(img->fd, hdr, sizeof(hdr)) != (int) sizeof(hdr)) {
		error("Image ends inside chunk %u\n", i);
		return -1;
	}

	c->type = get_unaligned_le16(hdr);
	c->len = (unsigned long long) get_unaligned_le32(hdr + 4) * img->blk_sz;
	total = get_unaligned_le32(hdr + 8);
	if (total < img->chunk_hdr_sz) {
		error("Chunk %u is smaller than its header\n", i);
		return -1;
	}
	body = total - img->chunk_hdr_sz;

	if (img->chunk_hdr_sz > sizeof(hdr) &&
	    lseek(img->fd, img->chunk_hdr_sz - sizeof(hdr), SEEK_CUR) < 0) {
		error("Failed to seek in the image: %d\n", errno);
		return -1;
	}

	switch (c->type) {
	case ANDROID_SPARSE_CHUNK_RAW:
		if (body != c->len) {
			error("Chunk %u says %llu bytes but has %llu\n", i,
			      c->len, body);
			return -1;
		}
		break;

	case ANDROID_SPARSE_CHUNK_FILL:
		if (body != sizeof(c->fill) ||
		    read_full(img->fd, c->fill, sizeof(c->fill)) !=
		    (int) sizeof(c->fill)) {
			error("Chunk %u isn't a fill\n", i);
			return -1;
		}
		break;

	case ANDROID_SPARSE_CHUNK_DONT_CARE:
		if (body) {
			error("Chunk %u is a hole with data in it\n", i);
			return -1;
		}
		break;

	case ANDROID_SPARSE_CHUNK_CRC32:
		if (lseek(img->fd, body, SEEK_CUR) < 0) {
			error("Failed to seek in the image: %d\n", errno);
			return -1;
		}
		break;

	default:
		error("Unknown chunk type 0x%x in chunk %u\n", c->type, i);
		return -1;
	}

	return 0;
}

#endif /* _SMOLUTILS_ANDROID_SPARSE_IMG_H */
