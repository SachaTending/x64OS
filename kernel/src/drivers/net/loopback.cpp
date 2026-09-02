#include <net/net.hpp>
#include <logging.hpp>
#include <fs/devtmpfs.h>

static Logger log("loopback");

static void loopback_transmitpacket(struct net_adapter *device, const void *data, size_t length) {
    struct net_packet *packet = new struct net_packet;
    packet->len = length;
    packet->data = (uint8_t *)malloc(length);
    memcpy(packet->data, data, length);

    // immediately dump this packet back on itself
    spinlock_acquire(&device->cachelock);
    device->cache.push_back(packet);
    spinlock_release(&device->cachelock);
    event_trigger(&device->packetevent, false);
}

static void loopback_updateflags(struct net_adapter *device, uint16_t old) {
    (void)old;

    device->flags |= IFF_RUNNING; // force running
}

void loopback_init() {
    log.info("Loopback is starting up...\n");
    net_adapter *dev = (net_adapter *)Resource::Create(sizeof(net_adapter));
    
    dev->can_mmap = false;
    dev->stat.st_mode = 0666 | S_IFCHR;
    dev->stat.st_rdev = resource_create_dev_id();
    dev->ioctl = net_ifioctl;

    dev->hwmtu = 0;
    dev->flags |= IFF_LOOPBACK | IFF_RUNNING;

    dev->txpacket = loopback_transmitpacket;
    dev->updateflags = loopback_updateflags;

    dev->type = NET_ADAPTERETH | NET_ADAPTERLO; // Ethernet-based Loopback device
    Net::Register(dev);

    dev->cachelock = (spinlock_t)SPINLOCK_INIT;

    dev->ip = NET_IPSTRUCT(NET_IP(127, 0, 0, 1));
    dev->subnetmask = NET_IPSTRUCT(NET_IP(255, 0, 0, 0));

    devtmpfs_add_device((struct resource *)dev, dev->ifname);
}