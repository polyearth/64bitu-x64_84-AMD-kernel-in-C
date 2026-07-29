#include "desktop.h"
#include "gui.h"
#include "mouse.h"

// Piekļuve tava kodola mainīgajiem
extern volatile int ui_mode;
extern void print(const char* text);

// Globālie peles mainīgie (tiem jābūt volatile!)
extern volatile int mouse_x;
extern volatile int mouse_y;
extern volatile int mouse_left_clicked;

// === JAUNIE MAINĪGIE PELIETIM ===
// Šie mainīgie atceras, kurā pozīcijā pele REĀLI šobrīd ir uzzīmēta ekrānā
static int drawn_x = 40;
static int drawn_y = 12;

// 1. Darbība, ko veiks mūsu faila ikona
void open_editor_action(void) {
    ui_mode = 2; // Pārslēdzamies uz tavu Redaktora režīmu!

    // Notīrām ekrānu un uzzīmējam redaktora paziņojumu
    gui_draw_rect(0, 0, 80, 25, 0x1F); // Zils fons, balti burti redaktoram
    gui_draw_text(2, 1, "=== REDAKTORS (Notepad) ===", 0x1F);
    gui_draw_text(2, 3, "Spied F2 lai saglabatu, ESC lai izietu.", 0x1F);
}

// 2. Izveidojam pašu ikonu
static OS_Icon desktop_icons[1];
static int icon_count = 0;

void desktop_init(void) {
    desktop_icons[0].x = 4;   // Noliekam smuki sākumā
    desktop_icons[0].y = 3;
    desktop_icons[0].w = 3;   // Platums 3 zīmes
    desktop_icons[0].h = 4;   // Augstums 4 rindas (3 lapa + 1 teksts)
    desktop_icons[0].color = 0x0F; // Tīri balta papīra lapa uz melna fona

    int i = 0;
    char* name = "TESTS.TXT";
    while(name[i] != '\0' && i < 11) {
        desktop_icons[0].label[i] = name[i];
        i++;
    }
    desktop_icons[0].label[i] = '\0';

    desktop_icons[0].action = open_editor_action;
    icon_count = 1;

    // Sinhronizējam sākuma koordinātas ar reālo peles starta pozīciju
    drawn_x = mouse_x;
    drawn_y = mouse_y;
    
    // Uzzīmējam peli pirmo reizi
    gui_show_mouse();
}

// Uzzīmē visas darbvirsmas ikonas
void desktop_draw(void) {
    for (int i = 0; i < icon_count; i++) {
        
        gui_draw_icon(desktop_icons[i].x, desktop_icons[i].y,
                      desktop_icons[i].label, desktop_icons[i].color);
    }
}

// === JAUNĀ FUNKCIJA: Kustības apstrāde un "astes" dzēšana ===
void desktop_update_mouse(void) {
    // Ja volatile mainīgie no interapta atšķiras no tā, ko uzzīmējām pēdējā kadrā
    if (mouse_x != drawn_x || mouse_y != drawn_y) {
        
        // 1. Drošības pēc saglabājam jauno interapta pozīciju lokāli
        int next_x = mouse_x;
        int next_y = mouse_y;

        // 2. Uz brīdi "apmānām" sistēmu un pasakām, ka pele joprojām ir vecajā vietā,
        // lai gui_hide_mouse() iztīrītu tieši tos pikseļus/simbolus, kur pele reāli stāv
        mouse_x = drawn_x;
        mouse_y = drawn_y;
        gui_hide_mouse(); 

        // 3. Tagad iestatām jauno pozīciju un uzzīmējam peli tur
        mouse_x = next_x;
        mouse_y = next_y;
        gui_show_mouse();

        // 4. Piefiksējam atmiņā, ka veiksmīgi pārlaidām peli uz jauno punktu
        drawn_x = next_x;
        drawn_y = next_y;
    }
}

// Pārbauda, vai pele klikšķa brīdī atradās virs kādas ikonas
void check_desktop_clicks(void) {
    if (mouse_left_clicked) {
        for (int i = 0; i < icon_count; i++) {
            OS_Icon* icon = &desktop_icons[i];

            // LABOTS: Bounding Box tagad izmanto drawn_x/drawn_y (to, ko lietotājs redz),
            // nevis tiešos interapta datus, kas klikšķa mikrosekundē varēja jau paspēt pakustēties.
            if (drawn_x >= icon->x && drawn_x < (icon->x + icon->w) &&
                drawn_y >= icon->y && drawn_y < (icon->y + icon->h)) {

                // Nodzēšam peli no ekrāna pirms GUI režīma maiņas
                mouse_x = drawn_x;
                mouse_y = drawn_y;
                gui_hide_mouse();

                // Izpildām ikonas darbību!
                icon->action();
                break;
            }
        }
        // Resetējam klikšķi
        mouse_left_clicked = 0;
    }
}
