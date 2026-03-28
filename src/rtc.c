#include "rtc.h"
#include "io.h"

uint8_t get_rtc_register(int reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

void get_time_string(char* buf) {
    // Wait for "update in progress" flag to clear to avoid reading garbage
    uint32_t timeout = 1000;
    while ((get_rtc_register(0x0A) & 0x80) && timeout > 0) {
        timeout--;
    }
    if (timeout == 0) return; // Skip if RTC is unresponsive

    uint8_t second = get_rtc_register(0x00);
    uint8_t minute = get_rtc_register(0x02);
    uint8_t hour   = get_rtc_register(0x04);
    uint8_t registerB = get_rtc_register(0x0B);

    // Convert BCD to binary if necessary
    if (!(registerB & 0x04)) {
        second = (second & 0x0F) + ((second / 16) * 10);
        minute = (minute & 0x0F) + ((minute / 16) * 10);
        hour   = ((hour & 0x0F) + (((hour & 0x70) / 16) * 10)) | (hour & 0x80);
    }

    // Simple formatting HH:MM
    buf[0] = (hour / 10) + '0';
    buf[1] = (hour % 10) + '0';
    buf[2] = ':';
    buf[3] = (minute / 10) + '0';
    buf[4] = (minute % 10) + '0';
    buf[5] = '\0';
}