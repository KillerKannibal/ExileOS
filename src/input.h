#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

extern volatile int mouse_x;
extern volatile int mouse_y;
extern volatile uint8_t mouse_left;
extern volatile uint8_t mouse_cycle;
extern uint8_t mouse_byte[3];

extern uint8_t shift_pressed;
extern uint8_t ctrl_pressed;

void mouse_wait(uint8_t type);
void mouse_write(uint8_t data);
uint8_t mouse_read();
void mouse_install();
void mouse_update(int dx, int dy, uint8_t buttons);
void mouse_handler();

extern unsigned char keyboard_map[128];
extern unsigned char keyboard_map_shifted[128];

void keyboard_handler();

#endif