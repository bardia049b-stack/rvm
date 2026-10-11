#ifndef RVM_NET_USER_H
#define RVM_NET_USER_H

#include "../rvm.h"

typedef struct net_user net_user;

typedef bool (*net_user_output_fn)(void *ud, const u8 *frame, u32 len);

rvm_err net_user_init(net_user **out, net_user_output_fn out_fn, void *out_ud);
void net_user_free(net_user *n);

void net_user_input(net_user *n, const u8 *frame, u32 len);

void net_user_poll(net_user *n);

#endif
