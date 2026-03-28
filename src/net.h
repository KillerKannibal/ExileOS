#ifndef NET_H
#define NET_H

#include <stdint.h>

// Generic structure for any network card (PCI or USB)
typedef struct net_device {
    char name[16];
    uint8_t mac[6];
    void (*send_packet)(void* data, uint32_t len);
    struct net_device* next;
} net_device_t;

extern net_device_t* net_devices;

void net_init();
void net_register_device(net_device_t* dev);

// Ethernet Frame Header
typedef struct {
    uint8_t dest[6];
    uint8_t src[6];
    uint16_t type;
} __attribute__((packed)) ethernet_frame_t;

#endif