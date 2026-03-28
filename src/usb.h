#ifndef USB_H
#define USB_H

#include <stdint.h>

// Standard USB Mouse Boot Protocol Packet
typedef struct {
    uint8_t buttons;
    int8_t  x_offset;
    int8_t  y_offset;
    int8_t  wheel; // Only present in some HID profiles
} __attribute__((packed)) usb_mouse_packet_t;

#define USB_MOUSE_BTN_LEFT   (1 << 0)
#define USB_MOUSE_BTN_RIGHT  (1 << 1)
#define USB_MOUSE_BTN_MIDDLE (1 << 2)

#endif