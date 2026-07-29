#ifndef VGA_H
#define VGA_H

// 1. Pieliekam VGA ekrāna izmēru definīcijas
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

// Mūsu VGA moduļa funkcijas
void clear_screen(void);
void put_char_at(char c, char color, int x, int y);
void draw_box(int start_x, int start_y, int width, int height, char color);
void print(const char *str);

// Globālie mainīgie, kas dzīvo kernel.c
extern int cursor_x;
extern int cursor_y;
extern char current_color;

// 2. Pasakām vga.c failam, ka šīs funkcijas ir definētas kernel.c failā
// un tās drīkst droši izsaukt!
extern void update_cursor(int x, int y);
extern void draw_menu_bar(void);
extern void check_scrolling(void);

#endif
