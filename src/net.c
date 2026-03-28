#include "net.h"
#include "shell.h"
#include "string.h"

net_device_t* net_devices = 0;

void net_init() {
    term_print("Network Stack: Initializing Layer 2 (Ethernet)...");
}

void net_register_device(net_device_t* dev) {
    term_print("Network: Registered new interface:");
    term_print(dev->name);

    // Add to linked list of devices
    if (!net_devices) {
        net_devices = dev;
    } else {
        net_device_t* curr = net_devices;
        while (curr->next) curr = curr->next;
        curr->next = dev;
    }
}