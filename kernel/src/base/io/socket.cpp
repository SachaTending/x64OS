#include <net/net.hpp>
#include <sched/sched.hpp>
#include <logging.hpp>
#include <errno.h>
#include <io/socket.hpp>
#include <netinet/in.h>

static Logger log("Socket");

struct socket *socket_create_udp(int type, int protocol);

static bool stub_bind(struct socket *thiz, struct f_description *description, void *addr, socklen_t len) {
    (void)thiz;
    (void)description;
    (void)addr;
    (void)len;
    // errno = ENOSYS;
    return false;
}

static bool stub_connect(struct socket *thiz, struct f_description *description, void *addr, socklen_t len) {
    (void)thiz;
    (void)description;
    (void)addr;
    (void)len;
    // errno = ENOSYS;
    return false;
}

static bool stub_getpeername(struct socket *thiz, struct f_description *description, void *addr, socklen_t *len) {
    (void)thiz;
    (void)description;
    (void)addr;
    (void)len;
    // errno = ENOSYS;
    return false;
}

static bool stub_getsockname(struct socket *thiz, struct f_description *description, void *addr, socklen_t *len) {
    (void)thiz;
    (void)description;
    (void)addr;
    (void)len;
    // errno = ENOSYS;
    return false;
}

static bool stub_listen(struct socket *thiz, struct f_description *description, int backlog) {
    (void)thiz;
    (void)description;
    (void)backlog;
    // errno = ENOSYS;
    return false;
}

static struct socket *stub_accept(struct socket *thiz, struct f_description *description, struct socket *other, void *addr, socklen_t *len) {
    (void)thiz;
    (void)description;
    (void)other;
    (void)addr;
    (void)len;
    // errno = ENOSYS;
    return NULL;
}

static ssize_t stub_recvmsg(struct socket *thiz, struct f_description *description, struct msghdr *msg, int flags) {
    (void)thiz;
    (void)description;
    (void)msg;
    (void)flags;
    // errno = ENOSYS;
    return -1;
}

static ssize_t stub_sendmsg(struct socket *thiz, struct f_description *description, const struct msghdr *msg, int flags) {
    (void)thiz;
    (void)description;
    (void)msg;
    (void)flags;
    // errno = ENOSYS;
    return -1;
}

static ssize_t stub_getsockopt(struct socket *thiz, struct f_description *description, int level, int optname, void *optval, socklen_t *optlen) {
    (void)thiz;
    (void)description;
    (void)level;
    (void)optname;
    (void)optval;
    (void)optlen;
    // errno = ENOSYS;
    return -1;
}

static ssize_t stub_setsockopt(struct socket *thiz, struct f_description *description, int level, int optname, const void *optval, socklen_t optlen) {
    (void)thiz;
    (void)description;
    (void)level;
    (void)optname;
    (void)optval;
    (void)optlen;
    // errno = ENOSYS;
    return -1;
}

void *socket_create(int family, int type, int protocol, int size) {
    struct socket *sock = CREATE_RESOURCE(struct socket);
    if (sock == NULL) {
        return NULL;
    }
    sock->stat.st_mode = S_IFSOCK;

    sock->state = SOCKET_CREATED;
    sock->family = family;
    sock->type = type;
    sock->protocol = protocol;

    sock->bind = stub_bind;
    sock->connect = stub_connect;
    sock->getpeername = stub_getpeername;
    sock->getsockname = stub_getsockname;
    sock->listen = stub_listen;
    sock->accept = stub_accept;
    sock->recvmsg = stub_recvmsg;
    sock->sendmsg = stub_sendmsg;
    sock->getsockopt = stub_getsockopt;
    sock->setsockopt = stub_setsockopt;

    return (struct socket *)sock;
}

int fdnum_create_from_resource(thread_t *proc, struct resource *res, int flags,
                               int old_fdnum, bool specific);

int syscall_socket(struct thread *caller, int family, int type, int protocol) {
    log.debug("socket(%d, %d, %d);\n", family, type, protocol);

    size_t socktype = type;
    int flags = 0;
    if (type & SOCK_CLOEXEC) {
        flags |= O_CLOEXEC;
        socktype -= SOCK_CLOEXEC;
    }
    if (type & SOCK_NONBLOCK) {
        flags |= O_NONBLOCK;
        socktype -= SOCK_NONBLOCK;
    }

    struct socket *sock;
    switch (family) {
        // TODO: AF_UNIX
        case AF_INET:
            switch (socktype) {
                case SOCK_STREAM:
                    if (protocol == 0) {
                        protocol = IPPROTO_TCP;
                    }
                    log.debug("got SOCK_STREAM, but currently this is unimplemented.\n");
                    return -EINVAL;
                case SOCK_DGRAM:
                    if (protocol == 0) {
                        protocol = IPPROTO_UDP;
                    }
                    log.debug("got SOCK_DGRAM\n");
                    sock = socket_create_udp(socktype, protocol);
                    break;
                default:
                    log.debug("invalid socket type: %d\n", socktype);
                    return -EINVAL;
            }
            break;
        default:
            log.debug("invalid socket family: %d\n", family);
            return -EINVAL;
    }
    if (sock == NULL) {
        return -EINVAL;
    }
    int ret = fdnum_create_from_resource(caller, (struct resource *)sock, flags, 0, false);

    if (ret < 0) {
        // TODO: Free socket
        return -EFAULT;
    }
    log.debug("Created socket fd=%d\n", ret);
    return ret;
}