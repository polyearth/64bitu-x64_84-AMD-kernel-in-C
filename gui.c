#include "gui.h"
#include "mouse.h"

// Piekļuve tavām esošajām VGA funkcijām
extern void put_char_at(char burts, char krasa, int x, int y);

// Saglabājam iepriekšējo peles pozīciju, lai zinātu, ko nodzēst
static int last_mouse_x = -1;
static int last_mouse_y = -1;
static uint8_t saved_attr = 0x07; // Standarta balts uz melna

// Funkcija, kas uzzīmē krāsainu taisnstūri (piem. fonam vai logam)
void gui_draw_rect(int x, int y, int w, int h, uint8_t color_attr) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            // Aizpildām ar tukšumu, bet iedodam krāsas atribūtu
            put_char_at(' ', color_attr, x + j, y + i);
        }
    }
}

// Funkcija teksta izvadīšanai konkrētā GUI vietā
void gui_draw_text(int x, int y, const char* text, uint8_t color_attr) {
    int idx = 0;
    while (text[idx] != '\0') {
        put_char_at(text[idx], color_attr, x + idx, y);
        idx++;
    }
}

// Uzzīmē smuku teksta ikonu (Bilde augšā, teksts apakšā)
void gui_draw_icon(int x, int y, const char* label, uint8_t icon_color) {
put_char_at(218, icon_color, x, y);     
    put_char_at(196, icon_color, x + 1, y); 
    put_char_at(191, icon_color, x + 2, y); 
    
    // Rinda 2: Lapas vidus ar "teksta rindas" imitāciju │■│
    put_char_at(179, icon_color, x, y + 1); 
    put_char_at(254, icon_color, x + 1, y + 1); 
    put_char_at(179, icon_color, x + 2, y + 1); 
    
    // Rinda 3: Lapas apakša └─┘
    put_char_at(192, icon_color, x, y + 2); 
    put_char_at(196, icon_color, x + 1, y + 2); 
    put_char_at(217, icon_color, x + 2, y + 2); 
    
    // Rinda 4: Nosaukums centrēts zem lapas (parastā pelēkā krāsā 0x07)
    gui_draw_text(x - 2, y + 3, label, 0x07);
}

// Parāda peli, invertējot krāsas pašreizējā punktā
void gui_show_mouse(void) {
    volatile uint8_t* vid_mem = (volatile uint8_t*)0xB8000;
    
    // Aprēķinām adresi atribūta baitam (katrs simbols aizņem 2 baitus: [char][attr])
    int offset = (mouse_y * 80 + mouse_x) * 2 + 1;
    
    // Saglabājam oriģinālo atribūtu un pozīciju
    saved_attr = vid_mem[offset];
    last_mouse_x = mouse_x;
    last_mouse_y = mouse_y;
    
    // Invertējam krāsas (apmainām priekšplānu ar fonu)
    uint8_t fg = saved_attr & 0x0F;
    uint8_t bg = (saved_attr & 0xF0) >> 4;
    vid_mem[offset] = (fg << 4) | bg;
}

// Atjauno veco krāsu, kad pele pakustas prom
void gui_hide_mouse(void) {
    if (last_mouse_x == -1 || last_mouse_y == -1) return;
    
    volatile uint8_t* vid_mem = (volatile uint8_t*)0xB8000;
    int offset = (last_mouse_y * 80 + last_mouse_x) * 2 + 1;
    
    vid_mem[offset] = saved_attr;
}
