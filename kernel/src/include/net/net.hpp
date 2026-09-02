#pragma once
#include <fs/resource.h>
#include <net/if.h>
#include <stdint.h>
#include <sys/socket.h>
#include <frg/vector.hpp>
#include <frg/std_compat.hpp>
#include <new>
// The following code was copied from lyre os project


#define NET_PORTRANGESTART 49152
#define NET_PORTRANGEEND UINT16_MAX

struct net_inetaddr {
    union {
        struct {
            uint8_t data[4]; // encapsulate so union shares all this memory instead of individual elements
        };
        uint32_t value;
    };
};

struct net_macaddr {
    uint8_t mac[6];
} __attribute__((packed));

struct net_packet { // high-level kernel interface for packets
    size_t len;
    uint8_t *data;
};

struct net_inethwpair {
    struct net_inetaddr inet;
    struct net_macaddr hw;
};

struct net_etherframe {
    struct net_macaddr dest;
    struct net_macaddr src;
    uint16_t type; // Big endian
    uint8_t data[]; // data needs to be in a dynamic representation
} __attribute__((packed));

enum {
    NET_ADAPTERETH = (1 << 0),
    NET_ADAPTERLO = (1 << 1)
};

typedef frg::vector<struct net_inethwpair *, frg::stl_allocator> addrcache_t;
typedef frg::vector<struct net_packet *, frg::stl_allocator> net_cache_t;
typedef frg::vector<struct socket *, frg::stl_allocator> bound_socks_t;

#define NET_IP(a, b, c, d) (((uint32_t)d << 24) | ((uint32_t)c << 16) | ((uint32_t)b << 8) | ((uint32_t)a << 0))
#define NET_IPSTRUCT(a) ({ (struct net_inetaddr) { .value = (a) }; })
#define NET_PRINTIP(a) (a).data[0], (a).data[1], (a).data[2], (a).data[3]
#define NET_MACSTRUCT(a, b, c, d, e, f) ({ (struct net_macaddr) { .mac = { a, b, c, d, e, f } }; })
#define NET_PRINTMAC(a) (a).mac[0], (a).mac[1], (a).mac[2], (a).mac[3], (a).mac[4], (a).mac[5]

#define NET_ETHPROTOIPV4 0x800
#define NET_ETHPROTOARP 0x806

struct net_adapter : resource {

    struct net_macaddr mac;
    struct net_macaddr permmac;
    struct net_inetaddr ip;
    struct net_inetaddr gateway;
    struct net_inetaddr subnetmask;
    uint16_t ipframe;
    uint16_t flags;
    int index;
    size_t hwmtu; // hardware driver MTU
    size_t mtu;
    addrcache_t adddrcache; // keep a record (per adapter as they may be connected to different networks) of IP-to-MAC records (from the ARP cache)
    spinlock_t addrcachelock;
    net_cache_t cache;
    spinlock_t cachelock; // not the same as the char device lock
    char ifname[IFNAMSIZ];
    uint8_t type;
    struct event packetevent; // signal for packet arrival

    spinlock_t socklock; // lock on socket
    bound_socks_t boundsocks;

    void (*txpacket)(struct net_adapter *adapter, const void *data, size_t length);
    void (*updateflags)(struct net_adapter *adapter, uint16_t old);
};

namespace Net
{
    void Init();
    void Register(struct net_adapter *adapter);
} // namespace Net

int net_ifioctl(struct resource *_this, struct f_description *description, uint64_t request, uint64_t arg);