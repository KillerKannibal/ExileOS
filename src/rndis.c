#include "rndis.h"
#include "net.h"
#include "string.h"
#include "shell.h"
#include "debug.h"

static net_device_t rndis_dev;
static int rndis_state = RNDIS_UNINITIALIZED;

void rndis_send_packet(void* data, uint32_t len) {
    if (rndis_state != RNDIS_DATA_READY) return;

    // RNDIS works by wrapping the Ethernet frame with a 44-byte header
    // This would then be sent via a USB Bulk Out transfer.
    kprint_serial("[RNDIS] Wrapping Ethernet frame for USB transfer...\n");
    
    // Future: usb_bulk_write(endpoint_out, encapsulated_data, len + 44);
}

void rndis_init() {
    kprint_serial("[BOOT] Initializing RNDIS (USB Tethering) Driver...\n");

    strcpy(rndis_dev.name, "usb0");
    
    // Default MAC - In production, this is queried via OID_802_3_CURRENT_ADDRESS
    rndis_dev.mac[0] = 0x02;
    rndis_dev.mac[1] = 0x00;
    rndis_dev.mac[2] = 0x00;
    rndis_dev.mac[3] = 0xDE;
    rndis_dev.mac[4] = 0xAD;
    rndis_dev.mac[5] = 0x01;

    rndis_dev.send_packet = rndis_send_packet;
    rndis_dev.next = 0;

    net_register_device(&rndis_dev);
    
    // Transition to ready once the USB handshake is complete
    rndis_state = RNDIS_DATA_READY; 
    term_print("Network: RNDIS USB Tethering interface registered.");
}