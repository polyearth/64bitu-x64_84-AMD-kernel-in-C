#include "vga.h"

// Ja tev VGA zīmēšanai bija vajadzīgi globālie mainīgie (piemēram, cursor_x, cursor_y),
// pārliecinies, ka tie ir pieejami arī šeit.

void clear_screen(void) {
for (int y = 0; y < 25; y++) {
        for (int x = 0; x < 80; x++) {
            volatile char *video = (volatile char*)0xB8000;
            int index = (y * 80 + x) * 2;
            video[index]     = ' ';     // Tukšums
            video[index + 1] = 0x0F;    // Melns fons, balts teksts
        }
    }
    cursor_x = 0;
    cursor_y = 0;
    update_cursor(0, 0);
}

void put_char_at(char c, char color, int x, int y) {
   char *vga = (char *)0xB8000;
    int index = (y * VGA_WIDTH + x) * 2;
    vga[index] = c;
    vga[index + 1] = color;
}

void draw_box(int start_x, int start_y, int width, int height, char color) {
    // 1. Aizpildām loga iekšpusi ar tukšumu un izvēlēto krāsu
    for (int y = start_y; y < start_y + height; y++) {
        for (int x = start_x; x < start_x + width; x++) {
            if (x >= 0 && x < VGA_WIDTH && y >= 0 && y < 25) {
                put_char_at(' ', color, x, y);
            }
        }
    }

    // 2. Sazīmējam stūrus (izmantojot to int vērtības no Code Page 437)
    put_char_at(218, color, start_x, start_y);                  // Augšējais kreisais ┌
    put_char_at(191, color, start_x + width - 1, start_y);      // Augšējais labais ┐
    put_char_at(192, color, start_x, start_y + height - 1);      // Apakšējais kreisais └
    put_char_at(217, color, start_x + width - 1, start_y + height - 1);  // Apakšējais labais ┘

    // 3. Sazīmējam malas
    for (int x = start_x + 1; x < start_x + width - 1; x++) {
        put_char_at(196, color, x, start_y);               // Augšējā mala ─
        put_char_at(196, color, x, start_y + height - 1);  // Apakšējā mala ─
    }
    for (int y = start_y + 1; y < start_y + height - 1; y++) {
        put_char_at(179, color, start_x, y);               // Kreisā mala │
        put_char_at(179, color, start_x + width - 1, y);   // Labā mala │
    }
}


void print(const char *str) {
    int i = 0;
    while (str[i] != '\0') {
        if (str[i] == '\n') {
            cursor_x = 0;
            cursor_y++;
            check_scrolling();
        } else {
            put_char_at(str[i], current_color, cursor_x, cursor_y);
            cursor_x++;
            if (cursor_x >= VGA_WIDTH) {
                cursor_x = 0;
                cursor_y++;
                check_scrolling();
            }
        }
        i++;
    }
    update_cursor(cursor_x, cursor_y);
}
