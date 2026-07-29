#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

// Globālie mainīgie peles stāvoklim
extern volatile int mouse_x;
extern volatile int mouse_y;
extern volatile int mouse_left_clicked;

// Funkcijas
void mouse_init(void);
void mouse_handler(void); 

#endif
