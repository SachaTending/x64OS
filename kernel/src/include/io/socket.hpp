// This code(and socket.cpp) was copied from lyre os and modified for x64OS
#pragma once
#include <fs/resource.h>
#include <sys/socket.h>
#include <net/net.hpp>

enum socket_state {
    SOCKET_CREATED,
    SOCKET_BOUND,
    SOCKET_LISTENING,
    SOCKET_CONNECTED,
    SOCKET_CLOSED,
};

typedef struct socket: resource_t {
    struct sockaddr_storage localaddr;
    struct sockaddr_storage peeraddr;

    struct socket **backlog;
    size_t backlog_max;
    size_t backlog_i;

    struct event connect_event;
    enum socket_state state;
    struct socket *peer;
    bool bound;

    int family;
    int type;
    int protocol;

    bool (*bind)(struct socket *thiz, struct f_description *description, void *addr, socklen_t len);
    bool (*connect)(struct socket *thiz, struct f_description *description, void *addr, socklen_t len);
    bool (*getpeername)(struct socket *thiz, struct f_description *description, void *addr, socklen_t *len);
    bool (*getsockname)(struct socket *thiz, struct f_description *description, void *addr, socklen_t *len);
    bool (*listen)(struct socket *thiz, struct f_description *description, int backlog);
    struct socket *(*accept)(struct socket *thiz, struct f_description *description, struct socket *other, void *addr, socklen_t *len);
    ssize_t (*recvmsg)(struct socket *thiz, struct f_description *description, struct msghdr *msg, int flags);
    ssize_t (*sendmsg)(struct socket *thiz, struct f_description *description, const struct msghdr *msg, int flags);
    ssize_t (*getsockopt)(struct socket *thiz, struct f_description *description, int level, int optname, void *optval, socklen_t *optlen);
    ssize_t (*setsockopt)(struct socket *thiz, struct f_description *description, int level, int optname, const void *optval, socklen_t optlen);
} socket_t;

typedef struct inetsocket: socket_t {
    struct net_adapter *adapter;

    be_uint16_t port; // redundancy
    be_uint16_t destport; // redundancy
    bool canbroadcast;
    bool canroute;
} inetsocket_t;

void *socket_create(int family, int type, int protocol, int size);