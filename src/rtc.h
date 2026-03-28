#ifndef RTC_H
#define RTC_H

#include <stdint.h>

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

uint8_t get_rtc_register(int reg);
void get_time_string(char* buf);

#endif