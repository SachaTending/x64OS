#include <net/net.hpp>
#include <libc.h>
#include <sched/sched.hpp>
#include <logging.hpp>
#include <arch/arch.hpp>
#include <linux/sockios.h>
#include <errno.h>

static Logger log("Network");

frg::vector<struct net_adapter *, frg::stl_allocator> net_adapters;

static int net_ethcount = 0;

static uint8_t *net_portbitmap; // use a bitmap to keep track of port allocations

net_adapter *ifhandler_arg;
bool is_ifhandler_spinup;

void ifhandler_thread() {
    net_adapter *adapter = ifhandler_arg;
    is_ifhandler_spinup = true;   
    log.debug("ifhandler thread started for %s\n", adapter->ifname);
    for(;;) {
        while (adapter->cache.size() == 0) {
            struct event *events[] = { &adapter->packetevent };
            event_await(events, 1, true);
        }

        spinlock_acquire(&adapter->cachelock);
        struct net_packet *packet = adapter->cache.pop(); // grab latest
 
        struct net_etherframe *ethframe = NULL; // ethernet frame (in case of ethernet)
        if (adapter->type & NET_ADAPTERETH) { // ethernet
            ethframe = (struct net_etherframe *)packet->data;
        }
        //adapter->cache.pop() // remove from cache to give us more space
        spinlock_release(&adapter->cachelock);

        if (adapter->type & NET_ADAPTERETH) { // ethernet
            switch (__builtin_bswap16(ethframe->type)) {
                case NET_ETHPROTOIPV4: // IPv4
                    //net_oninet(adapter, ethframe->data, packet->len - sizeof(struct net_etherframe));
                    log.debug("ifhandler: got ipv4 packet\n");
                    free(packet);
                    free(packet->data);
                    break;
                case NET_ETHPROTOARP: // ARP
                    //net_onarp(adapter, ethframe->data, packet->len - sizeof(struct net_etherframe));
                    log.debug("ifhandler: got arp packet\n");
                    free(packet);
                    free(packet->data);
                    break;
                default:
                    free(packet);
                    free(packet->data);
                    break;
            }
        }
    }
}



int net_ifioctl(struct resource *_this, struct f_description *description, uint64_t request, uint64_t arg) {
    struct ifreq *req = (struct ifreq *)arg;
    if (req->ifr_ifru.ifru_ivalue) {
        if (request == SIOCGIFNAME) { // this one relies on the index instead of the name
            struct net_adapter *thiz = NULL; // adapter in question
            for (auto net_adapt : net_adapters) {
                if (net_adapt->index == req->ifr_ifru.ifru_ivalue) {
                    thiz = net_adapt;
                }
            };

            if (thiz == NULL) {
                // XXX: Should there be an errno?
                errno = ENODEV;
                return -1;
            }

            strncpy(req->ifr_ifrn.ifrn_name, thiz->ifname, IFNAMSIZ);
            return 0;
        }
    }

    struct net_adapter *thiz = NULL; // adapter in question
    switch (request) {
        default: {
            for (auto net_adapt : net_adapters) {
                if (!strncmp(net_adapt->ifname, req->ifr_ifrn.ifrn_name, IFNAMSIZ)) {
                    thiz = net_adapt;
                }
            };

            if (thiz == NULL) {
                for (auto net_adapt : net_adapters) { // if there is no matching interface name, try match based on interface index
                    if (net_adapt->index == req->ifr_ifru.ifru_ivalue) {
                        thiz = net_adapt;
                    }
                };

                if (thiz == NULL) {
                    // XXX: Should there be an errno?
                    errno = ENODEV;
                    return -1;
                }
            }

        }
    }

    switch (request) {
        /*
        case SIOCGIFGATEWAY: { // XXX: Make a gateway ioctl
            struct sockaddr_in *inaddr = (struct sockaddr_in *)&req->ifr_addr;
            inaddr->sin_family = AF_INET;
            inaddr->sin_addr.s_addr = this->gateway.value;
            return 0;
        }
        case SIOCSIFGATEWAY: { // XXX: Make a gateway ioctl
            struct sockaddr_in *inaddr = (struct sockaddr_in *)&req->ifr_addr;
            if (inaddr->sin_family != AF_INET) {
                errno = EPROTONOSUPPORT;
                return -1;
            }

            this->gateway.value = inaddr->sin_addr.s_addr;

            return 0;
        }
        case SIOCGIFFLAGS: {
            req->ifr_ifru.ifru_flags = this->flags;
            return 0;
        }
        case SIOCSIFFLAGS: {
            uint16_t old = this->flags;
            this->flags = req->ifr_ifru.ifru_flags;
            this->updateflags(this, old); // update for flags
            return 0;
        }
        case SIOCSIFNAME: {
            char devpath[32];
            snprintf(devpath, 32, "/dev/%s", this->ifname);
            vfs_unlink(vfs_root, devpath); // unlink original
            strncpy(this->ifname, req->ifr_newname, IFNAMSIZ);
            devtmpfs_add_device((struct resource *)this, this->ifname); // add new one
            return 0;
        }
        case SIOCGIFMTU: {
            req->ifr_ifru.ifru_mtu = this->mtu;
            return 0;
        }
        case SIOCSIFMTU: {
            if (this->hwmtu) {
                if (this->mtu > this->hwmtu) {
                    errno = EINVAL;
                    return -1; // MTU is over the maximum the hardware can handle
                }

                if (req->ifr_ifru.ifru_mtu) {
                    this->mtu = req->ifr_ifru.ifru_mtu; // will only set to the requested MTU if it's not zero
                } else {
                    errno = EINVAL;
                    return -1;
                }
            } else {
                this->mtu = req->ifr_ifru.ifru_mtu; // will set to whatever the dynamic MTU adapter can handle
            }
            return 0;
        }
        case SIOCGIFADDR: {
            struct sockaddr_in *inaddr = (struct sockaddr_in *)&req->ifr_addr;
            inaddr->sin_family = AF_INET;
            inaddr->sin_addr.s_addr = this->ip.value;
            return 0;
        }
        case SIOCSIFADDR: {
            struct sockaddr_in *inaddr = (struct sockaddr_in *)&req->ifr_addr;
            if (inaddr->sin_family != AF_INET) {
                errno = EPROTONOSUPPORT;
                return -1;
            }

            this->ip.value = inaddr->sin_addr.s_addr;
            return 0;
        }
        case SIOCGIFNETMASK: {
            struct sockaddr_in *inaddr = (struct sockaddr_in *)&req->ifr_netmask;
            inaddr->sin_family = AF_INET;
            inaddr->sin_addr.s_addr = this->subnetmask.value;
            return 0;
        }
        case SIOCSIFNETMASK: {
            struct sockaddr_in *inaddr = (struct sockaddr_in *)&req->ifr_netmask;

            if (inaddr->sin_family != AF_INET) {
                errno = EPROTONOSUPPORT;
                return -1;
            }

            this->subnetmask.value = inaddr->sin_addr.s_addr;
            return 0;
        }
        case SIOCGIFHWADDR: {
            memcpy(req->ifr_hwaddr.sa_data, this->mac.mac, sizeof(struct net_macaddr));
            return 0;
        }
        case SIOCGIFINDEX: {
            req->ifr_ifru.ifru_ivalue = this->index;
            return 0;
        }
        */
    }

    return resource_default_ioctl(_this, description, request, arg);
}

void Net::Register(struct net_adapter *adapter) {
    if (adapter->type & NET_ADAPTERLO) {
        adapter->mtu = 0; // loopback interfaces do not care about MTU
        strcpy(adapter->ifname, "lo");
    } else if (adapter->type & NET_ADAPTERETH) {
        adapter->mtu = 1500; // this can be changed!
        snprintf(adapter->ifname, IFNAMSIZ, "eth%d", net_ethcount++);
    }

    //VECTOR_PUSH_BACK(&net_adapters, adapter);
    net_adapters.push_back(adapter);
    adapter->index = net_adapters.size();

    //adapter->addrcache = (typeof(adapter->addrcache))VECTOR_INIT;
    //adapter->cache = (typeof(adapter->cache))VECTOR_INIT;
    adapter->cachelock = (spinlock_t)SPINLOCK_INIT;
    adapter->addrcachelock = (spinlock_t)SPINLOCK_INIT;

    ifhandler_arg = adapter;
    char buf[512];
    snprintf(buf, 512, "Net ifhandler thread for %s", adapter->ifname);
    is_ifhandler_spinup = false;
    Scheduler::CreateThread(buf, ifhandler_thread, false, krnl_page);
    while (is_ifhandler_spinup == false) HALT;

}
void loopback_init();
void Net::Init() {
    log.info("Network subsystem is starting up...\n");
    net_portbitmap = new uint8_t[NET_PORTRANGEEND - NET_PORTRANGESTART];
    loopback_init();
}