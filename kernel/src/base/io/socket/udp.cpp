// This code was copied from lyre os and modified for x64OS
#include <io/socket.hpp>
#include <netinet/in.h>
#include <logging.hpp>
#include <frg/std_compat.hpp>
#include <frg/vector.hpp>
#include <new>
#include <poll.h>

static Logger log("UDP");

typedef frg::vector<struct udp_packet *, frg::stl_allocator> udp_packets_t;

struct udp_socket: inetsocket_t {

    uint32_t recenttimestamp;
    udp_packets_t *packets; // udp packets
};
int net_ifioctl(struct resource *_this, struct f_description *description, uint64_t request, uint64_t arg);

static spinlock_t udp_socketslock = SPINLOCK_INIT;
frg::vector<struct udp_socket *, frg::stl_allocator> udp_sockets;

static bool udp_acquireport(struct udp_socket *sock, uint16_t port) {
    if (!port) {
        //errno = EINVAL; // invalid port
        return false;
    }

    spinlock_acquire(&udp_socketslock);
    udp_sockets.push(sock);
    spinlock_release(&udp_socketslock);
    return true;
}
/*
static bool udp_bind(struct socket *_this, struct f_description *description, void *addr_, socklen_t len) {
    (void)description;
    (void)len;

    struct udp_socket *thiz = (struct udp_socket *)_this;

    struct sockaddr_in *addr = (struct sockaddr_in *)addr_;
    if (addr->sin_family != AF_INET) {
        //errno = EINVAL;
        return false;
    }

    struct net_adapter *a = NULL;
    if (addr->sin_addr.s_addr == INADDR_ANY) { // on all interfaces
        if (thiz->adapter) {
            net_unbindsocket(thiz->adapter, (struct socket *)thiz);
        }

        thiz->adapter = NULL; // since we want all interfaces
    } else if ((a = net_findadapterbyip((struct net_inetaddr) { .value = addr->sin_addr.s_addr }))) {
        Net::BindSocket(a, (struct socket *)thiz);
    } else {
        errno = -EADDRNOTAVAIL;
        return false;
    }

    memcpy(&thiz->localaddr, addr, sizeof(struct sockaddr_in));

    thiz->port = addr->sin_port; // assign listening port
    if (!thiz->port) { // 0 port (bind to any port)
        thiz->port = __builtin_bswap16(net_allocport());
    }

    bool ret = udp_acquireport(thiz, __builtin_bswap16(thiz->port)); // final state success depends on the ability to attain access to this port
    if (!ret) {
        if (thiz->adapter) {
            net_unbindsocket(thiz->adapter, (struct socket *)thiz);
        }
    } else {
        thiz->bound = true;
    }
    return ret;
}*/

static bool udp_connect(struct socket *_this, struct f_description *description, void *_addr, socklen_t len) {
    (void)description;
    (void)len;

    struct udp_socket *thiz = (struct udp_socket *)_this;

    struct sockaddr_in *addr = (struct sockaddr_in *)_addr;

    spinlock_acquire(&thiz->lock);
    // mark address and socket state
    memcpy(&thiz->peeraddr, addr, sizeof(struct sockaddr_in));

    thiz->state = SOCKET_CONNECTED;
    thiz->status |= POLLIN | POLLOUT;
    spinlock_release(&thiz->lock);

    event_trigger(&thiz->event, false);

    return true;
}

struct socket *socket_create_udp(int type, int protocol) {
    if (protocol != IPPROTO_UDP) {
        return NULL;
    }

    struct udp_socket *sock = (struct udp_socket *)socket_create(AF_INET, type, protocol, sizeof(struct udp_socket));
    if (sock == NULL) return NULL;

    sock->status = SOCKET_CREATED;
    sock->family = AF_INET;
    sock->type = type;
    sock->protocol = protocol;
    sock->packets = new udp_packets_t();
    sock->stat.st_mode = S_IFSOCK;

    /*
    sock->read = udp_read;
    sock->write = udp_write;
    sock->unref = udp_unref;
    sock->ioctl = net_ifioctl;

    sock->bind = udp_bind;
    sock->connect = udp_connect; // connect to socket (will now become default address)
    sock->getpeername = udp_getpeername;
    sock->getsockname = udp_getsockname;
    sock->recvmsg = udp_recvmsg;
    sock->sendmsg = udp_sendmsg;
    sock->getsockopt = udp_getsockopt;
    sock->setsockopt = udp_setsockopt;
    */

    sock->ioctl = net_ifioctl;

    sock->connect = udp_connect;

    return (struct socket *)sock;
}