#include "pci.h"
#include "io.h"
#include "shell.h"
#include "utils.h"
#include "string.h"

uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    uint32_t tmp = 0;

    // Create configuration address
    address = (uint32_t)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));

    // Write out the address
    outl(PCI_CONFIG_ADDRESS, address);
    // Read in the data
    tmp = inl(PCI_CONFIG_DATA);
    return tmp;
}

void pci_init() {
    term_print("Scanning PCI bus for USB controllers...");

    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            for (int func = 0; func < 8; func++) {
                uint32_t vendor_device = pci_config_read(bus, slot, func, 0);
                if ((vendor_device & 0xFFFF) == 0xFFFF) continue;

                uint32_t class_info = pci_config_read(bus, slot, func, 0x08);
                uint8_t class_code = (class_info >> 24) & 0xFF;
                uint8_t subclass   = (class_info >> 16) & 0xFF;
                uint8_t prog_if    = (class_info >> 8) & 0xFF;

                if (class_code == PCI_CLASS_SERIAL_BUS && subclass == PCI_SUBCLASS_USB) {
                    char msg[64] = "Found USB Controller: ";
                    if (prog_if == 0x00) strcat(msg, "UHCI (USB 1.1)");
                    else if (prog_if == 0x10) strcat(msg, "OHCI (USB 1.1)");
                    else if (prog_if == 0x20) strcat(msg, "EHCI (USB 2.0)");
                    else if (prog_if == 0x30) strcat(msg, "xHCI (USB 3.0)");
                    else strcat(msg, "Unknown Type");
                    
                    term_print(msg);
                }
            }
        }
    }
}