#include <stdint.h>
#include "keyboard.h"
#include "vga.h"

extern void outb(unsigned short port, unsigned char val);
extern unsigned char inb(unsigned short port);

// Kopējie mainīgie no kernel.c
extern volatile int ui_mode;
extern int cursor_x;
extern int cursor_y;
extern char current_color;
extern char cmd_buffer[256];
extern int cmd_index;
extern unsigned char kbd_us[128];

// Izvēlņu mainīgie
extern int selected_menu;
extern int menu_open;
extern int selected_item;
extern void draw_menu_bar(void);
extern volatile int command_ready;
extern void desktop_draw(void);

// === JAUNIE MAINĪGIE REDAKTORAM ===
volatile char last_pressed_char = 0;
volatile int new_char_available = 0;

// Caps Lock stāvoklis (0 = mazie burti, 1 = lielie burti)
int caps_lock_active = 0;

extern void editor_handle_input(char c); // Redaktora burtu apstrāde
extern void gui_draw_rect(int x, int y, int w, int h, unsigned char color);
extern void process_mouse_byte(uint8_t data);

// Šī funkcija satur visu tavu režīmu un loģikas kodu
void process_keyboard_scancode(uint8_t scancode) {
    if (scancode == 0x3A) {
        caps_lock_active = !caps_lock_active;
        return;
    }

    static int test_flip = 0;
    if (test_flip) {
        put_char_at('K', 0x4F, 79, 0);
        test_flip = 0;
    } else {
        put_char_at(' ', 0x0F, 79, 0);
        test_flip = 1;
    }

    if (!(scancode & 0x80)) {
        // ==========================================
        // REŽĪMS 2: REDAKTORS (Notepad)
        // ==========================================
        if (ui_mode == 2) {
            if (scancode == 0x01) {
                // ESC -> Nosūtām 27 uz redaktoru, lai tas apstrādā iziešanu
                editor_handle_input(27);
            }
            else if (scancode == 0x3C) {
                // F2 -> Nosūtām 1, lai sāktu saglabāšanu
                editor_handle_input(1);
            }
            else if (scancode == 0x1C) {
                // ENTER -> Nosūtām 13 jaunai rindai
                editor_handle_input(13);
            }
            else if (scancode == 0x0E) {
                // BACKSPACE -> Nosūtām 8 dzēšanai
                editor_handle_input(8);
            }
            else {
                // Parastie burti un cipari
                char burts = kbd_us[scancode];
                if (burts != 0) {
                    if (caps_lock_active && (burts >= 'a' && burts <= 'z')) {
                        burts -= 32;
                    }
                    editor_handle_input(burts);
                }
            }
            return;
        }
        // ==========================================
        // REŽĪMS 3: DARBVIRSMA (Desktop)
        // ==========================================
        else if (ui_mode == 3) {
            if (scancode == 0x01) {
                ui_mode = 0;
                clear_screen();
                print("Atgriezamies KristapsOS konsole...\n> ");
                cmd_index = 0;
            }
        }
        // ==========================================
        // REŽĪMS 4: INTERAKTĪVAIS LOGS (UI režīms)
        // ==========================================
        else if (ui_mode == 4) {
            if (scancode == 0x01) { // ESC -> Aizveram logu un atgriežamies konsolē
                ui_mode = 0;
                clear_screen();
                draw_menu_bar();
                print("> ");
                cmd_index = 0;
                return;
            }
            
            if (scancode == 0x1C) { // ENTER -> Iztīrām loga ievades lauku jaunai rakstīšanai
                cmd_buffer[cmd_index] = '\0';
                
                // Iztīrām loga ievades joslu vizuāli (no X=19 līdz X=62, rindā 11)
                for (int x = 19; x < 63; x++) {
                    put_char_at(' ', 0x70, x, 11);
                }
                cursor_x = 19;
                cursor_y = 11;
                update_cursor(cursor_x, cursor_y);
                cmd_index = 0;
                return;
            }
            
            if (scancode == 0x0E) { // BACKSPACE loga ietvaros
                if (cmd_index > 0) {
                    cmd_index--;
                    cursor_x--;
                    put_char_at(' ', 0x70, cursor_x, cursor_y);
                    update_cursor(cursor_x, cursor_y);
                }
                return;
            }
            
            // Parasto burtu rakstīšana loga iekšienē
            char burts = kbd_us[scancode];
            if (burts != 0) {
                // Neļaujam iziet ārpus loga labās malas (X=62) un bufera izmēra
                if (cursor_x < 62 && cmd_index < 250) {
                    if (caps_lock_active && (burts >= 'a' && burts <= 'z')) {
                        burts -= 32;
                    }
                    cmd_buffer[cmd_index] = burts;
                    cmd_index++;
                    put_char_at(burts, 0x70, cursor_x, cursor_y); // 0x70 ir loga pelēkais fons ar melnu tekstu
                    cursor_x++;
                    update_cursor(cursor_x, cursor_y);
                }
            }
            return;
        }
        // ==========================================
        // REŽĪMS 1: KONSOLĒS IZVĒLNE (MENU)
        // ==========================================
        else if (ui_mode == 1) {
            if (scancode == 0x01) { ui_mode = 0; draw_menu_bar(); update_cursor(cursor_x, cursor_y); }
            else if (menu_open == 0) {
                if (scancode == 0x4D) { if (selected_menu < 1) { selected_menu++; draw_menu_bar(); } }
                else if (scancode == 0x4B) { if (selected_menu > 0) { selected_menu--; draw_menu_bar(); } }
                else if (scancode == 0x1C) { menu_open = 1; selected_item = 0; draw_menu_bar(); }
            }
            else {
                if (scancode == 0x50) { if (selected_item < 5) { selected_item++; draw_menu_bar(); } }
                else if (scancode == 0x48) { if (selected_item > 0) { selected_item--; draw_menu_bar(); } }
                else if (scancode == 0x01) { menu_open = 0; draw_menu_bar(); }
                else if (scancode == 0x1C) {
                    if (selected_menu == 1 && selected_item == 0) { clear_screen(); ui_mode = 0; menu_open = 0; draw_menu_bar(); print("> "); }
                    else if (selected_menu == 1 && selected_item == 1) { ui_mode = 0; menu_open = 0; draw_menu_bar(); print("\n"); cmd_buffer[0] = 'c'; cmd_buffer[1] = 'p'; cmd_buffer[2] = 'u'; cmd_buffer[3] = '\0'; cmd_index = 3; execute_command(); }
                    else if (selected_menu == 1 && selected_item == 2) { ui_mode = 0; menu_open = 0; draw_menu_bar(); print("\n"); cmd_buffer[0] = 'h'; cmd_buffer[1] = 'e'; cmd_buffer[2] = 'l'; cmd_buffer[3] = 'p'; cmd_buffer[4] = '\0'; cmd_index = 4; execute_command(); }
                    else if (selected_menu == 1 && selected_item == 3) { ui_mode = 0; menu_open = 0; draw_menu_bar(); print("\n"); cmd_buffer[0] = 'a'; cmd_buffer[1] = 'b'; cmd_buffer[2] = 'o'; cmd_buffer[3] = 'u'; cmd_buffer[4] = 't'; cmd_buffer[5] = '\0'; cmd_index = 5; execute_command(); }
                    else if (selected_menu == 1 && selected_item == 4) { ui_mode = 0; menu_open = 0; draw_menu_bar(); print("\n"); cmd_buffer[0] = 'v'; cmd_buffer[1] = 'e'; cmd_buffer[2] = 'r'; cmd_buffer[3] = 's'; cmd_buffer[4] = 'i'; cmd_buffer[5] = 'o'; cmd_buffer[6] = 'n'; cmd_buffer[7] = '\0'; cmd_index = 7; execute_command(); }
                    else if (selected_menu == 1 && selected_item == 5) { ui_mode = 0; menu_open = 0; draw_menu_bar(); print("\n"); cmd_buffer[0] = 'u'; cmd_buffer[1] = 'i'; cmd_buffer[2] = '\0'; cmd_index = 2; execute_command(); }
                }
            }
        }
        // ==========================================
        // REŽĪMS 0: PARASTĀ KOMANDRINDA
        // ==========================================
        else {
            if (scancode == 0x3B) { ui_mode = 1; draw_menu_bar(); }
            else if (scancode == 0x1C) { execute_command(); }
            else if (scancode == 0x0E) {
                if (cmd_index > 0) {
                    cmd_index--; cursor_x--;
                    put_char_at(' ', current_color, cursor_x, cursor_y);
                    update_cursor(cursor_x, cursor_y);
                }
            }
            else {
                char burts = kbd_us[scancode];
                if (burts != 0 && cmd_index < 254) {
                    if (caps_lock_active && (burts >= 'a' && burts <= 'z')) burts -= 32;
                    cmd_buffer[cmd_index] = burts;
                    cmd_index++;
                    char temp[2] = {burts, '\0'};
                    print(temp);
                }
            }
        }
    }
}

// Aparatūras interapts (IRQ1)
void keyboard_handler(void) {
    uint8_t status = inb(0x64);

    while (status & 0x01) {
        uint8_t data = inb(0x60);

        if (!(status & 0x20)) {
            process_keyboard_scancode(data);
        } else {
            process_mouse_byte(data);
        }

        status = inb(0x64);
    }

    outb(0x20, 0x20);
}
