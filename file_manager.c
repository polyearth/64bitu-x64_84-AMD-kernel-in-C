#include "fat.h"

// Pieņemam standarta VGA teksta režīma ekrāna funkcijas no tava GUI
extern void gui_draw_rect(int x, int y, int w, int h, uint8_t color);
extern void print_at(const char* str, int x, int y, uint8_t color);
extern uint16_t current_dir_cluster;
extern int ata_read_sector(uint32_t lba, uint8_t *buffer);
extern uint32_t get_dir_lba(uint16_t cluster, uint32_t* sectors_count);

extern void put_char_at(char c, char color, int x, int y);

extern volatile int ui_mode;
extern void desktop_draw(void);
// Glabāsim pašreizējā logā redzamos failus, lai pele zinātu, kur uzklikšķināts
typedef struct {
    char name[13];
    int is_dir;
    uint16_t cluster;
} ClickableItem;

static ClickableItem file_list[32];
static int file_count = 0;


void print_at(const char* str, int x, int y, uint8_t color) {
    while (*str) {
        put_char_at(*str, color, x, y);
        x++;
        str++;
    }
}

// Funkcija, kas uzzīmē File Manager logu
void draw_file_manager(void) {
    // Zīmējam loga rāmi (piemēram, pelēks fons ar zilu galveni)
    gui_draw_rect(5, 2, 70, 20, 0x70);  // Pelēks logs
    gui_draw_rect(5, 2, 70, 1, 0x1F);   // Zila augšējā josla
    print_at(" FAILU PARLUKS (File Manager) ", 6, 2, 0x1F);
    print_at(" [X] ", 70, 2, 0x4F);     // Aizvēršanas poga

    // Nolasām failus no diska un sagatavojam sarakstu
    uint32_t sectors_count = 0;
    uint32_t dir_lba = get_dir_lba(current_dir_cluster, &sectors_count);
    uint8_t sector_buf[512];
    FAT16_DirEntry* entry;
    
    file_count = 0;

    for (uint32_t s = 0; s < sectors_count && file_count < 32; s++) {
        if (ata_read_sector(dir_lba + s, sector_buf) != 0) return;
        for (int i = 0; i < 16 && file_count < 32; i++) {
            entry = (FAT16_DirEntry*)&sector_buf[i * 32];
            if (entry->filename[0] == 0x00) break;
            if ((uint8_t)entry->filename[0] == 0xE5 || (entry->attributes & 0x08)) continue;

            // Saglabājam faila/mapes struktūru priekš peles klikšķiem
            file_list[file_count].is_dir = (entry->attributes & 0x10) ? 1 : 0;
            file_list[file_count].cluster = entry->first_cluster_low;

            // Uzbūvējam nosaukumu
            char full_name[13];
            int k = 0;
            for(int j=0; j<8; j++) if(entry->filename[j] != ' ') full_name[k++] = entry->filename[j];
            if (!(entry->attributes & 0x10) && entry->ext[0] != ' ') {
                full_name[k++] = '.';
                for(int j=0; j<3; j++) if(entry->ext[j] != ' ') full_name[k++] = entry->ext[j];
            }
            full_name[k] = '\0';
            
            for(int m=0; m<13; m++) file_list[file_count].name[m] = full_name[m];

            // Izvadām uz ekrāna File Manager logā
            int row = 4 + file_count;
            if (file_list[file_count].is_dir) {
                print_at("[DIR] ", 7, row, 0x71); // Zili burti mapēm
                print_at(full_name, 13, row, 0x71);
            } else {
                print_at("[FIL] ", 7, row, 0x70); // Melni burti failiem
                print_at(full_name, 13, row, 0x70);
            }
            file_count++;
        }
    }
}

// Funkcija, kas reaģē uz peles klikšķi File Manager logā
void file_manager_handle_click(int mx, int my) {
    // Pārbaudām, vai uzklikšķināts uz aizvēršanas pogas [X] (X=70, Y=2)
    if (mx >= 70 && mx <= 73 && my == 2) {
        ui_mode = 3; // Atpakaļ uz parasto Desktop
        desktop_draw();
        return;
    }

    // Pārbaudām klikšķus uz failu saraksta rindām
    if (mx >= 7 && mx <= 40 && my >= 4 && my < (4 + file_count)) {
        int selected_idx = my - 4;
        ClickableItem item = file_list[selected_idx];

        if (item.is_dir) {
            // Ja uzspiež uz mapes, ieejam tajā (analogi CLI komandai 'cd')
            current_dir_cluster = item.cluster;
            draw_file_manager(); // Pārzīmējam logu ar jauno saturu
        } else {
            // Ja uzspiež uz faila, AUTOMĀTISKI palaižam Notepad un ielādējam tekstu!
            extern void editor_open_file_path(const char* path);
            editor_open_file_path(item.name);
        }
    }
}
