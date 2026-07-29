#ifndef GUI_H
#define GUI_H

#include <stdint.h>

// Zīmēšanas funkcijas
void gui_draw_rect(int x, int y, int w, int h, uint8_t color_attr);
void gui_draw_text(int x, int y, const char* text, uint8_t color_attr);
void gui_draw_icon(int x, int y, const char* label, uint8_t icon_color);

// Peles kursora loģika
void gui_show_mouse(void);
void gui_hide_mouse(void);

#endif
