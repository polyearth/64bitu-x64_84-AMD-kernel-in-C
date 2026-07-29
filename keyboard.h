#ifndef KEYBOARD_H
#define KEYBOARD_H

// Galvenā funkcija
void keyboard_handler(void);

// Izvēlņu stāvokļi, kas palika kernel.c
extern int menu_open;
extern int selected_menu;
extern int selected_item;
extern volatile int ui_mode;

// Komandu buferis un tā indekss no kernel.c
extern char cmd_buffer[];
extern int cmd_index;

// Klaviatūras izkārtojuma (scancode) masīvs no kernel.c
extern unsigned char kbd_us[];

// Funkcijas, kuras tastatūra mēdz izsaukt
extern void draw_menu_bar(void);
extern void draw_dropdown_menu(int menu_idx);
extern void execute_command(void);

#endif
