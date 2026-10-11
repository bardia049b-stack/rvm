#ifndef RVM_VIRTIO_NET_H
#define RVM_VIRTIO_NET_H

#include "virtio.h"

#define VIRTIO_NET_F_MAC     5
#define VIRTIO_NET_F_STATUS  16
#define VIRTIO_NET_S_LINK_UP 1

#define VIRTIO_NET_HDR_SIZE 12

typedef struct virtio_net virtio_net;

rvm_err virtio_net_init(virtio_net *n);
void virtio_net_shutdown(virtio_net *n);
virtio_backend *virtio_net_backend(virtio_net *n);

void virtio_net_poll(virtio_net *n);

u64 virtio_net_rx_packets(const virtio_net *n);
u64 virtio_net_tx_packets(const virtio_net *n);

struct virtio_net {
    virtio_backend be;
    virtio *vio;
    u8 mac[6];
    u16 status;
    u64 n_rx, n_tx, n_drop;
    void *user;
};
#endif
