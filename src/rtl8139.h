#ifndef RTL8139_H
#define RTL8139_H

#include <stdint.h>

#define RTL8139_VENDOR_ID 0x10EC
#define RTL8139_DEVICE_ID 0x8139

void rtl8139_init();
void rtl8139_send_packet(void* data, uint32_t len);

// This will be called by the interrupt handler
void rtl8139_handler();

#endif