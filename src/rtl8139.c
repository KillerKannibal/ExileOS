#include "rtl8139.h"
#include "pci.h"
#include "io.h"
#include "shell.h"
#include "string.h"

static uint32_t io_base;
static uint8_t mac_address[6];
static uint8_t rx_buffer[8192 + 16 + 1500]; // 8KB + header space + max packet size

void rtl8139_init() {
    // 1. Find the device via PCI
    uint8_t bus, slot, func;
    int found = 0;
    for (int b = 0; b < 256; b++) {
        for (int s = 0; s < 32; s++) {
            uint32_t id = pci_config_read(b, s, 0, 0);
            if ((id & 0xFFFF) == RTL8139_VENDOR_ID && (id >> 16) == RTL8139_DEVICE_ID) {
                bus = b; slot = s; func = 0;
                found = 1; break;
            }
        }
        if (found) break;
    }

    if (!found) {
        term_print("Network: RTL8139 NIC not found.");
        return;
    }

    // 2. Get I/O Base Address from BAR0
    io_base = pci_config_read(bus, slot, func, 0x10) & (~0x3);
    
    // 3. Enable PCI Bus Mastering
    // Note: In a real system, you'd use pci_config_write to set the Command register
    // QEMU usually has this enabled by default for the RTL8139.

    term_print("Network: Found RTL8139. Initializing...");

    // 4. Power on (Send 0 to Config1)
    outb(io_base + 0x52, 0x00);

    // 5. Software Reset
    outb(io_base + 0x37, 0x10);
    while((inb(io_base + 0x37) & 0x10) != 0);

    // 6. Init Receive Buffer
    memset(rx_buffer, 0, sizeof(rx_buffer));
    outl(io_base + 0x30, (uint32_t)&rx_buffer);

    // 7. Set Interrupt Mask (ROK: Receive OK, TOK: Transmit OK)
    outw(io_base + 0x3C, 0x0005);

    // 8. Configure Receive (Accept: AB+AM+APM+AAP - Broadcast, Multicast, Phys Match, All)
    outl(io_base + 0x44, 0xf | (1 << 7)); // WRAP bit set to 1

    // 9. Enable Receiver and Transmitter
    outb(io_base + 0x37, 0x0C);

    // 10. Read MAC Address
    for(int i=0; i<6; i++) mac_address[i] = inb(io_base + i);
    
    term_print("Network: RTL8139 Ready. MAC obtained.");
}

void rtl8139_handler() {
    uint16_t status = inw(io_base + 0x3E);
    outw(io_base + 0x3E, 0x05); // Acknowledge interrupts

    if (status & 0x01) { // Receive OK
        term_print("Network: Packet Received!");
        // Packet processing logic would go here
    }
}

static uint8_t tx_buffers[4][1536]; // 4 TX slots
static int current_tx_slot = 0;

void rtl8139_send_packet(void* data, uint32_t len) {
    memcpy(tx_buffers[current_tx_slot], data, len);
    
    // Set address and length/start bit
    outl(io_base + 0x20 + (current_tx_slot * 4), (uint32_t)tx_buffers[current_tx_slot]);
    outl(io_base + 0x10 + (current_tx_slot * 4), len);
    
    current_tx_slot = (current_tx_slot + 1) % 4;
}