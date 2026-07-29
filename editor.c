#include <stdint.h>
#include "vga.h"
#include "keyboard.h"

// Piekļuve VGA funkcijām un kursora kontrolei
extern void update_cursor(int x, int y);
extern void clear_screen(void);
extern void print(const char* str);
extern void put_char_at(char c, char color, int x, int y);
extern void desktop_draw(void);

extern int write_fat16_file(const char* name, const char* buffer, uint32_t data_len);

extern void gui_draw_rect(int x, int y, int w, int h, unsigned char color);
// No keyboard.c nākošie mainīgie
extern volatile char last_pressed_char;
extern volatile int new_char_available;
extern volatile int ui_mode;

// Ārējā deklarācija no fat.c
extern int load_fat16_file(const char* name, char* out_buffer, uint32_t max_len);

// === NOTEPAD ATMIŅA ===
#define MAX_TEXT_LEN 4096
char text_buffer[MAX_TEXT_LEN]; 
uint32_t text_len = 0;


// Saglabāšanas loga stāvoklis (0 - rakstām tekstu, 1 - ievadām faila nosaukumu)
static int save_mode_active = 0;
static char save_filename[40] = {0};
static int fn_len = 0;
static const int prompt_len = 25; // "Ieraksti faila nosaukumu: " garums

// === VIRTUALĀ FAILU SISTĒMA (RAM DISK) ===
#define MAX_FILES 10
#define MAX_FILE_SIZE 2048

typedef struct {
    char name[16];
    char data[MAX_FILE_SIZE];
    int length;
    int is_used;
} VirtualFile;

VirtualFile my_files[MAX_FILES];
int file_count = 0;

// Funkcija, ko izsauksim no komandrindas, lai redzētu failus
void list_files(void) {
    if (file_count == 0) {
        print("Nav neviena saglabata faila.\n> ");
        return;
    }

    print("Saglabatie faili:\n");
    for (int i = 0; i < MAX_FILES; i++) {
        if (my_files[i].is_used) {
            print("- ");
            print(my_files[i].name);
            print("\n");
        }
    }
    print("> ");
}

// Lokāla strcmp funkcija failu nosaukumu salīdzināšanai
int mans_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// Pārzīmē visu tekstu redaktora laukumā (Zils fons, balts teksts)
void redraw_editor_text(void) {
    // 1. Notīrām rakstīšanas laukumu ar tirkīza/zilu fonu (no 1. līdz 24. rindai)
    for (int y = 1; y < 25; y++) {
        for (int x = 0; x < 80; x++) {
            put_char_at(' ', 0x1F, x, y); 
        }
    }

    // 2. Izvadām tekstu no bufera
    int cur_x = 2;
    int cur_y = 2; // Sākam nedaudz atkāpjoties no malām smukumam

    for (uint32_t i = 0; i < text_len; i++) {
        char c = text_buffer[i];

        if (c == '\n' || c == 13) {
            cur_x = 2;
            cur_y++;
            if (cur_y >= 24) break;
        }
        else {
            put_char_at(c, 0x1F, cur_x, cur_y);
            cur_x++;
            if (cur_x >= 78) {
                cur_x = 2;
                cur_y++;
                if (cur_y >= 24) break;
            }
        }
    }

    // Ja neesam saglabāšanas režīmā, kursoru turam pie teksta beigām
    if (!save_mode_active) {
        update_cursor(cur_x, cur_y);
    }
}

// Funkcija, kas uzzīmē standarta augšējo rīkjoslu
void draw_editor_header(void) {
    for (int x = 0; x < 80; x++) {
        put_char_at(' ', 0x70, x, 0); // Pelēka josla
    }
    const char* title = " MicroEdit v0.1 | [F2] Saglabat | [ESC] Iziet uz Desktop";
    int t = 0;
    while (title[t] != '\0') {
        put_char_at(title[t], 0x70, t, 0);
        t++;
    }
}

// Šo funkciju izsauc no desktop.c, kad lietotājs uzklikšķina uz ikonas!
void start_editor(void) {
ui_mode = 2; // Pārslēdzamies uz redaktora režīmu
    text_len = 0;
    text_buffer[0] = '\0';  // Pilnībā notīram teksta buferi
    save_mode_active = 0;
    fn_len = 0;

    clear_screen();
    draw_editor_header();
    redraw_editor_text();
}

// === JAUNĀ UN SVARĪGĀKĀ FUNKCIJA: Apstrādā ievadi bez OS bloķēšanas ===
void editor_handle_input(char c) {
    if (!save_mode_active && c == 27) {
        ui_mode = 3;
        gui_draw_rect(0, 0, 80, 25, 0x07); // Melns Desktop fons
        desktop_draw();
        return;
    }

    
    // ---- 1. GADĪJUMS: Lietotājs ir nospiedis F2 un ievada faila nosaukumu ----
    if (save_mode_active) {
        if (c == 13) { // ENTER -> Apstiprināt saglabāšanu
            if (fn_len > 0) {
                save_filename[fn_len] = '\0';
                
                // Mēģinām rakstīt FAT16 sistēmā
                int status = write_fat16_file(save_filename, text_buffer, text_len);
                
                // Parādām statusu
                if (status == 0) {
                    const char* success_msg = " [ SAGLABATS VEIKSMIGI! ] ";
                    int m = 0;
                    while (success_msg[m] != '\0') {
                        put_char_at(success_msg[m], 0x2F, 45 + m, 0);
                        m++;
                    }
                } else {
                    const char* error_msg = " [ KLUDA SAGLABAJOT ] ";
                    int m = 0;
                    while (error_msg[m] != '\0') {
                        put_char_at(error_msg[m], 0x4F, 45 + m, 0);
                        m++;
                    }
                }
                
                // Neliela pauze, lai redz ziņojumu, un ejam atpakaļ uz Desktop
                for (volatile int delay = 0; delay < 30000000; delay++);
                
                ui_mode = 3;
                gui_draw_rect(0, 0, 80, 25, 0x07); // Melns Desktop fons
                desktop_draw();
            }
        }
        else if (c == 27) { // ESC -> Atcelt saglabāšanu, atgriezties pie teksta
            save_mode_active = 0;
            draw_editor_header();
            redraw_editor_text();
        }
        else if (c == 8) { // BACKSPACE faila nosaukumam
            if (fn_len > 0) {
                fn_len--;
                save_filename[fn_len] = '\0';
                put_char_at(' ', 0x30, prompt_len + fn_len, 0);
                update_cursor(prompt_len + fn_len, 0);
            }
        }
	else if (fn_len < 30 && ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '/' || c == '\\')) {
            // FAT16 failu nosaukumiem izmantojam lielos burtus
		if (c >= 'a' && c <= 'z') {
                c = c - 32;
            }
            // Ja ievada atpakaļvērsto slīpsvītru, nomainām uz parasto, lai vienkāršotu apstrādi
            if (c == '\\') {
                c = '/';
            }
            save_filename[fn_len] = c;
            put_char_at(c, 0x3F, prompt_len + fn_len, 0); 
            fn_len++;
            update_cursor(prompt_len + fn_len, 0);
        }
        return; // Beidzam apstrādi saglabāšanas režīmam
    }

    // ---- 2. GADĪJUMS: Parastā teksta rakstīšana zilajā logā ----
    if (c == 1) { // Mūsu pašu definētais F2 kods no keyboard.c
        save_mode_active = 1;
        fn_len = 0;
        save_filename[0] = '\0';

        // Pārzišam rīkjoslu ievades režīmā (gaiši zila josla)
        for (int x = 0; x < 80; x++) {
            put_char_at(' ', 0x30, x, 0);
        }
        const char* prompt = "Ieraksti faila nosaukumu: ";
        int p = 0;
        while (prompt[p] != '\0') {
            put_char_at(prompt[p], 0x30, p, 0);
            p++;
        }
        update_cursor(p, 0);
    }
    else if (c == 8) { // BACKSPACE tekstam
        if (text_len > 0) {
            text_len--;
            text_buffer[text_len] = '\0';
            redraw_editor_text();
        }
    }
    else if (c == 13 || c == '\n') { // ENTER
        if (text_len < MAX_TEXT_LEN - 1) {
            text_buffer[text_len] = '\n';
            text_len++;
            text_buffer[text_len] = '\0';
            redraw_editor_text();
        }
    }
    else if (c >= 32 && c <= 126) { // Drukājamie simboli
        if (text_len < MAX_TEXT_LEN - 1) {
            text_buffer[text_len] = c;
            text_len++;
            text_buffer[text_len] = '\0';
            redraw_editor_text();
        }
    }
}

// Funkcija failu lasīšanai
void read_file_content(const char* name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (my_files[i].is_used && mans_strcmp(my_files[i].name, name) == 0) {
            print(my_files[i].data);
            print("\n");
            return;
        }
    }
    print("Kluda: Fails netika atrasts!\n");
}



void editor_open_file_path(const char* path) {
    // 1. Notīrām veco stāvokli
    text_len = 0;
    text_buffer[0] = '\0';

    // 2. Mēģinām ielādēt failu no diska
    int bytes_read = load_fat16_file(path, text_buffer, 4000);
    
    if (bytes_read >= 0) {
        text_len = bytes_read;
    } else {
        // Ja nevarēja nolasīt, atveram kā jaunu/tukšu failu
        text_len = 0;
        text_buffer[0] = '\0';
    }

    // 3. Aktivizējam redaktora režīmu un pārzīmējam ekrānu
    ui_mode = 2; // Editor režīms
    save_mode_active = 0;
    
    clear_screen();
    draw_editor_header();
    redraw_editor_text();
}
