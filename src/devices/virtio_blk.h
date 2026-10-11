#ifndef RVM_VIRTIO_BLK_H
#define RVM_VIRTIO_BLK_H

#include "virtio.h"

#define VIRTIO_BLK_SECTOR_SIZE 512

#define VIRTIO_BLK_T_IN     0
#define VIRTIO_BLK_T_OUT    1
#define VIRTIO_BLK_T_FLUSH  4
#define VIRTIO_BLK_T_GET_ID 8

#define VIRTIO_BLK_S_OK     0
#define VIRTIO_BLK_S_IOERR  1
#define VIRTIO_BLK_S_UNSUPP 2

#define VIRTIO_BLK_F_BLK_SIZE 6
#define VIRTIO_BLK_F_FLUSH    9
#define VIRTIO_F_VERSION_1    32

typedef struct virtio_blk {
    virtio_backend be;
    int fd;
    u64 capacity;
    u32 blk_size;
    bool readonly;
    char id[21];

    u64 n_read, n_write, n_flush, n_err;
} virtio_blk;

rvm_err virtio_blk_open(virtio_blk *b, const char *path, u64 bytes, bool create);
void virtio_blk_close(virtio_blk *b);
virtio_backend *virtio_blk_backend(virtio_blk *b);

#endif
