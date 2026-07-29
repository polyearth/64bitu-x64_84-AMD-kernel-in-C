#include <stdint.h>
#include "editor.h"
#include "ata.h"
#include "keyboard.h"
#include "vga.h"
#define VGA_WIDTH 80
#include "mouse.h"
#include "gui.h"
#include "desktop.h"

int cursor_x = 0;
int cursor_y = 0;
char current_color = 0x0A; // Zaļš

char cmd_buffer[256];
//char cmd_buffer[1024] __attribute__((aligned(16)));
int cmd_index = 0;
extern volatile char last_pressed_char;
extern volatile int new_char_available;
extern void list_files(void);
extern void read_file_content(const char* name);

extern int ata_read_sector(uint32_t lba, uint8_t *buffer);
extern int ata_write_sector(uint32_t lba, const uint8_t *buffer);
extern void init_fat16(void);
extern void list_fat16_files(void);
extern void read_fat16_file(const char* name);
extern int delete_fat16_file(const char* name);

extern int mkdir_fat16(const char* name);
extern int change_directory(const char* name);



extern int mouse_x;
extern int mouse_y;
extern int mouse_left_clicked;

extern void mouse_init(void);
extern void gui_show_mouse(void);
extern void gui_hide_mouse(void);

extern void desktop_update_mouse(void);
extern void desktop_draw(void);
extern void check_desktop_clicks(void);


// UI Izvēlnes mainīgie
volatile int ui_mode = 0;         // 0 = parastā komandrinda, 1 = aktīva augšējā izvēlne
int selected_menu = 0;    // 0 = "System" izvēlne, 1 = "Tools" izvēlne
int menu_open = 0;        // 0 = nolaižamais logs ir aizvērts, 1 = atvērts
int selected_item = 0;    // Aktīvais punkts nolaižamajā logā (piem. 0, 1, 2)

void draw_menu_bar(void);


// 64-bitu IDT ieraksta struktūra (strikti jāatbilst x86_64 specifikācijai)
struct idt_entry {
    unsigned short offset_lowerbits;  // offset bits 0..15
    unsigned short selector;          // a code segment selector in GDT
    unsigned char  ist;               // bits 0..2 holds Interrupt Stack Table offset, rest of bits zero.
    unsigned char  type_attr;         // type and attributes
    unsigned short offset_midbits;    // offset bits 16..31
    unsigned int   offset_highbits;   // offset bits 32..63
    unsigned int   zero;              // reserved
} __attribute__((packed));

// 64-bitu IDT rādītājs (izmanto 8-baitu adresi)
struct idt_ptr {
    unsigned short limit;
    unsigned long base;               // 64-bitu adrese!
} __attribute__((packed));

// Pašu tabulu izveidojam ar 256 ierakstiem un tukšu rādītāju
struct idt_entry idt[256];
struct idt_ptr idtp;

// Funkcija, kas ieraksta konkretu adresi IDT tabulā
void idt_set_gate(int num, unsigned long base, unsigned short sel, unsigned char flags) {
    idt[num].offset_lowerbits = (base & 0xFFFF);
    idt[num].selector = sel;
    idt[num].ist = 0;
    idt[num].type_attr = flags;
    idt[num].offset_midbits = ((base >> 16) & 0xFFFF);
    idt[num].offset_highbits = ((base >> 32) & 0xFFFFFFFF);
    idt[num].zero = 0;
}



extern void keyboard_handler_asm(void); // ārpusēja funkcija
extern void mouse_handler_asm(void); // <-- PIEVIENO ŠO RINDU!
void outb(unsigned short port, unsigned char val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

// 2. Handlers
//void keyboard_handler(void) {
  //  outb(0x20, 0x20);
//}


void pic_remap(void) {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    // LABOTS: 
    // Master PIC (0x21): Atveram IRQ1 (tastatūra) UN IRQ2 (kaskāde uz Slave PIC) -> 0xF9
    outb(0x21, 0xF9);
    // Slave PIC (0xA1): Atveram IRQ12 (PS/2 pele) -> 0xEF
    outb(0xA1, 0xEF);
}


// Funkcija, kas reāli ielādē IDT procesorā
void idt_init(void) {
    // 1. VISPIRMS pilnībā iztīrām visu tabulu
    for(int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    // 2. Tikai TAGAD ierakstām mūsu reālos interaptus
    idt_set_gate(0x21, (unsigned long)keyboard_handler_asm, 0x08, 0x8E); // IRQ1
    idt_set_gate(0x2C, (unsigned long)mouse_handler_asm, 0x08, 0x8E);    // IRQ12

    // 3. Sagatavojam pointeri un ielādējam IDT
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base  = (unsigned long)&idt;

    pic_remap();

    // Izpildām asamblejas komandu lidt
    __asm__ volatile("lidt %0" : : "m"(idtp));
}






void testet_disku(void) {
    uint8_t rakstisanas_buf[512];
    uint8_t lasisanas_buf[512];

    print("\n--- SAKAM DISKA TESTU ---\n");

    // 1. Aizpildām rakstīšanas buferi ar nullēm
    for (int i = 0; i < 512; i++) {
        rakstisanas_buf[i] = 0;
    }

    // Ierakstām testa ziņojumu buferī
    const char* testa_zinojums = "KristapsOS ATA Driveris ir dzivs un ieraksta datus disk.img!";
    int j = 0;
    while (testa_zinojums[j] != '\0' && j < 500) {
        rakstisanas_buf[j] = testa_zinojums[j];
        j++;
    }

    print("Rakstam testa datus uz LBA 100...\n");
    if (ata_write_sector(100, rakstisanas_buf) == 0) {
        print("Ierakstisana pabeigta veiksmigi!\n");
    } else {
        print("KLUDA: Neizdevas ierakstit diskā!\n");
        return;
    }

    print("Lasam datus atpakal no LBA 100...\n");
    // Drošības pēc aizpildām lasīšanas buferi ar 'X', lai redzētu, vai tiešām dati tiks pārrakstīti
    for (int i = 0; i < 512; i++) {
        lasisanas_buf[i] = 'X';
    }

    if (ata_read_sector(100, lasisanas_buf) == 0) {
        print("Nolasīsana pabeigta! Satura sākums:\n\"");
        
        // Izvadām nolasīto tekstu (tikai pirmos 80 simbolus, lai neaizpildītu ekrānu ar drazu)
        char temp[2] = {0, 0};
        for (int i = 0; i < 80; i++) {
            if (lasisanas_buf[i] == 0) break; // Beidzam, ja nonākam līdz nulles simbolam
            temp[0] = lasisanas_buf[i];
            print(temp);
        }
        print("\"\n");
    } else {
        print("KLUDA: Neizdevas nolasit no diska!\n");
    }

    print("--- DISKA TESTS PABEIGTS ---\n> ");
}







//inline asamblejas funkcija, kas izvelk procesora nosaukumu
void get_cpu_vendor(char *vendor) {
    unsigned int ebx, edx, ecx;
    unsigned int eax = 0; // CPUID funkcija 0 atgriež Vendor string

    // Izpildām CPUID komandu. Tā ieraksta ražotāja burtus EBX, EDX un ECX reģistros
    __asm__ volatile("cpuid"
                     : "=b"(ebx), "=d"(edx), "=c"(ecx)
                     : "a"(eax));

    // Sadalām reģistrus pa baitiem un saliekam masīvā
    vendor[0] = (char)(ebx & 0xFF);
    vendor[1] = (char)((ebx >> 8) & 0xFF);
    vendor[2] = (char)((ebx >> 16) & 0xFF);
    vendor[3] = (char)((ebx >> 24) & 0xFF);

    vendor[4] = (char)(edx & 0xFF);
    vendor[5] = (char)((edx >> 8) & 0xFF);
    vendor[6] = (char)((edx >> 16) & 0xFF);
    vendor[7] = (char)((edx >> 24) & 0xFF);

    vendor[8] = (char)(ecx & 0xFF);
    vendor[9] = (char)((ecx >> 8) & 0xFF);
    vendor[10] = (char)((ecx >> 16) & 0xFF);
    vendor[11] = (char)((ecx >> 24) & 0xFF);
    
    vendor[12] = '\0'; // Nobeidzam teksta virkni
}

// Funkcija, kas izgūst pilno procesora modeļa nosaukumu
void get_cpu_brand(char *brand) {
    unsigned int eax, ebx, ecx, edx;
    
    // CPUID funkcijas no 0x80000002 līdz 0x80000004 dod procesora nosaukumu
    // Katra funkcija atgriež 16 baitus informācijas (kopā 48 baiti)
    for (unsigned int i = 0; i < 3; i++) {
        __asm__ volatile("cpuid"
                         : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                         : "a"(0x80000002 + i));
                         
        int offset = i * 16;
        
        // Ierakstām masīvā no EAX
        brand[offset + 0] = (char)(eax & 0xFF);
        brand[offset + 1] = (char)((eax >> 8) & 0xFF);
        brand[offset + 2] = (char)((eax >> 16) & 0xFF);
        brand[offset + 3] = (char)((eax >> 24) & 0xFF);
        
        // No EBX
        brand[offset + 4] = (char)(ebx & 0xFF);
        brand[offset + 5] = (char)((ebx >> 8) & 0xFF);
        brand[offset + 6] = (char)((ebx >> 16) & 0xFF);
        brand[offset + 7] = (char)((ebx >> 24) & 0xFF);
        
        // No ECX
        brand[offset + 8] = (char)(ecx & 0xFF);
        brand[offset + 9] = (char)((ecx >> 8) & 0xFF);
        brand[offset + 10] = (char)((ecx >> 16) & 0xFF);
        brand[offset + 11] = (char)((ecx >> 24) & 0xFF);
        
        // No EDX
        brand[offset + 12] = (char)(edx & 0xFF);
        brand[offset + 13] = (char)((edx >> 8) & 0xFF);
        brand[offset + 14] = (char)((edx >> 16) & 0xFF);
        brand[offset + 15] = (char)((edx >> 24) & 0xFF);
    }
    brand[48] = '\0'; // Nobeidzam tekstu
}



unsigned char kbd_us[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0,
    9, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 13,
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0
};



// Funkcija, kas nolasa krāsu atribūtu no konkrētām koordinātām
unsigned char get_color_at(int x, int y) {
    char *vga = (char *)0xB8000;
    int index = (y * VGA_WIDTH + x) * 2;
    return vga[index + 1]; // Atgriežam tikai krāsu bitus
}



//void put_char_at(char c, char color, int x, int y) 

void update_cursor(int x, int y) {
    unsigned short pos = y * VGA_WIDTH + x;
    __asm__ volatile("outb %0, %1" : : "a"((unsigned char)0x0F), "Nd"((unsigned short)0x3D4));
    __asm__ volatile("outb %0, %1" : : "a"((unsigned char)(pos & 0xFF)), "Nd"((unsigned short)0x3D5));
    __asm__ volatile("outb %0, %1" : : "a"((unsigned char)0x0E), "Nd"((unsigned short)0x3D4));
    __asm__ volatile("outb %0, %1" : : "a"((unsigned char)((pos >> 8) & 0xFF)), "Nd"((unsigned short)0x3D5));
}



void check_scrolling(void) {
    // Tā kā pēdējā rindiņa (Y=24) ir zilā statusa josla,
    // komandrindas teksts drīkst sasniegt maksimums Y=23.
    if (cursor_y >= 23) {
        char *vga = (char *)0xB8000;

        // Pabīdām rindiņas uz augšu, bet SĀKAM NO Y=2, nevis no Y=1!
        // Tādā veidā Y=1 rindiņa tiks pārrakstīta ar Y=2 rindiņas saturu,
        // bet pati pirmā rindiņa (Y=0, mūsu Menu) paliks pilnīgi neaiztikta.
        for (int y = 2; y < 24; y++) {
            for (int x = 0; x < VGA_WIDTH; x++) {
                int src_index = (y * VGA_WIDTH + x) * 2;
                int dest_index = ((y - 1) * VGA_WIDTH + x) * 2;
                
                vga[dest_index] = vga[src_index];         // Simbols
                vga[dest_index + 1] = vga[src_index + 1]; // Krāsa
            }
        }

        // Iztīrām tikai rindiņu Y=23 (kas ir pēdējā brīvā rindiņa pirms statusa joslas)
        for (int x = 0; x < VGA_WIDTH; x++) {
            int index = (23 * VGA_WIDTH + x) * 2;
            vga[index] = ' ';
            vga[index + 1] = current_color;
        }

        // Novietojam kursoru uz pēdējās atļautās rindiņas (Y=23)
        cursor_y = 23;
    }
}



//void print

//void clear_screen(void)


int strcmp(const char *str1, const char *str2) {
    int i = 0;
    while (str1[i] != '\0' && str2[i] != '\0') {
        if (str1[i] != str2[i]) return 1;
        i++;
    }
    if (str1[i] == '\0' && str2[i] == '\0') return 0;
    return 1;
}



//void init_idt(void) {
    // IDT netiek aiztikts
//}



//void draw_box(int start_x, int start_y, int width, int height, char color) {




void draw_window_border(int start_x, int start_y, int width, int height, char color) {
    // 1. Uzzīmējam četrus stūrus
    put_char_at(201, color, start_x, start_y);                  // Augšējais kreisais ╔
    put_char_at(187, color, start_x + width - 1, start_y);      // Augšējais labais ╗
    put_char_at(200, color, start_x, start_y + height - 1);     // Apakšējais kreisais ╚
    put_char_at(188, color, start_x + width - 1, start_y + height - 1); // Apakšējais labais ╝

    // 2. Uzzīmējam horizontālās līnijas (augšā un apakšā)
    for (int x = start_x + 1; x < start_x + width - 1; x++) {
        put_char_at(205, color, x, start_y);              // Augšējā mala ═
        put_char_at(205, color, x, start_y + height - 1); // Apakšējā mala ═
    }

    // 3. Uzzīmējam vertikālās līnijas (kreisajā un labajā malā)
    for (int y = start_y + 1; y < start_y + height - 1; y++) {
        put_char_at(186, color, start_x, y);             // Kreisā mala ║
        put_char_at(186, color, start_x + width - 1, y); // Labā mala ║
    }
}


void draw_window(int start_x, int start_y, int width, int height, char box_color, char title_color, const char *title) {
    int shadow_offset_x = 2; // Cik simbolus plata būs ēna labajā malā
    int shadow_offset_y = 1; // Cik rindiņas augsta būs ēna apakšā

    // 1. UZZĪMĒJAM ĒNU: Izmainām fona krāsu uz melnu (0x07) tiem simboliem, kas būs zem loga
    for (int y = start_y + shadow_offset_y; y < start_y + height + shadow_offset_y; y++) {
        for (int x = start_x + shadow_offset_x; x < start_x + width + shadow_offset_x; x++) {
            if (x >= 0 && x < VGA_WIDTH && y >= 0 && y < 25) {
                // Pārbaudām, vai šis punkts nepārklājas ar pašu jauno logu,
                // mēs gribam krāsot tikai to, kas "izvirzās" ārpus loga rāmja
                if (x >= start_x + width || y >= start_y + height) {
                    // Saglabājam esošo teksta krāsu, bet uzliekam MELNU fonu (0x0F maska un 0x00 fons)
                    char current_char_color = get_color_at(x, y);
                    char shadow_color = (current_char_color & 0x0F) | 0x00; 
                    
                    // Nolasām, kāds simbols tur jau stāvēja, lai to nesabojātu
                    char *vga = (char *)0xB8000;
                    char existing_char = vga[(y * VGA_WIDTH + x) * 2];
                    
                    // Ierakstām to pašu simbolu atpakaļ, bet ar tumšo ēnas krāsu
                    put_char_at(existing_char, shadow_color, x, y);
                }
            }
        }
    }

    // 2. UZZĪMĒJAM PAŠU LOGU (Tevis jau pārbaudītā loģika)
    draw_box(start_x, start_y, width, height, box_color);


    draw_window_border(start_x, start_y, width, height, 0x70);




    // Uzrakstām loga virsrakstu (title) augšējā rāmja vidū
    int title_len = 0;
    while (title[title_len] != '\0') title_len++;

    int title_x = start_x + (width - title_len) / 2;

    int i = 0;
    while (title[i] != '\0') {
        put_char_at(title[i], title_color, title_x + i, start_y);
        i++;
    }
}




void draw_menu_bar(void) {
    // Uzzīmējam pelēku joslu pa visu pirmo rindiņu (Y=0)
    // 0x70 ir melns teksts uz pelēka fona
    for (int x = 0; x < VGA_WIDTH; x++) {
        put_char_at(' ', 0x70, x, 0);
    }

    // Noteiksim krāsas atkarībā no tā, kurš punkts ir izvēlēts
    // 0x70 = parasts (melns uz pelēka), 0x0F = izgaismots (balts uz melna fona)
    char system_color = (selected_menu == 0 && ui_mode == 1) ? 0x0F : 0x70;
    char tools_color  = (selected_menu == 1 && ui_mode == 1) ? 0x0F : 0x70;

    // Uzrakstām "System" pogu pozīcijā X=2
    const char *menu1 = "  Sistema  ";
    int i = 0;
    while (menu1[i] != '\0') {
        put_char_at(menu1[i], system_color, 2 + i, 0);
        i++;
    }

    // Uzrakstām "Tools" pogu pozīcijā X=15
    const char *menu2 = "  Riki  ";
    i = 0;
    while (menu2[i] != '\0') {
        put_char_at(menu2[i], tools_color, 15 + i, 0);
        i++;
    }
    
    // Labajā stūrī vienmēr paturēsim mazu pamācību lietotājam
    const char *hint = "[F1: Menu] [ESC: Back]";
    i = 0;
    while (hint[i] != '\0') {
        put_char_at(hint[i], 0x74, 57 + i, 0); // 0x74 = sarkans uz pelēka
        i++;
    }



if (ui_mode == 1 && menu_open == 1) {
        if (selected_menu == 1) { // Ja izvēlēts "Tools"
            // Uzzīmējam mazu kasti tieši zem "Tools" (X=15, Y=1, platums=18, augstums=4)
            // 0x70 = melns uz pelēka
            draw_box(15, 1, 18, 8, 0x70);
            
            // Definējam divus punktus, ko rādīt sarakstā
            const char *item1 = "1. Tirit ekranu";
            const char *item2 = "2. CPU";
	    const char *item3 = "3. Palidziba";
	    const char *item4 = "4. Par";
            const char *item5 = "5. Versija";
	    const char *item6 = "6. UI";
            
            // Noteiksim, kura rindiņa ir izgaismota (0x0F = balts uz melna)
            char item1_color = (selected_item == 0) ? 0x0F : 0x70;
            char item2_color = (selected_item == 1) ? 0x0F : 0x70;
            char item3_color = (selected_item == 2) ? 0x0F : 0x70;
            char item4_color = (selected_item == 3) ? 0x0F : 0x70;
	    char item5_color = (selected_item == 4) ? 0x0F : 0x70;
	    char item6_color = (selected_item == 5) ? 0x0F : 0x70;

            // Ierakstām abus punktus kastē (X=16, Y=2 un Y=3)
            int s = 0;
            while (item1[s] != '\0') { put_char_at(item1[s], item1_color, 16 + s, 2); s++; }
            s = 0;
            while (item2[s] != '\0') { put_char_at(item2[s], item2_color, 16 + s, 3); s++; }
           s = 0;
             while (item3[s] != '\0') { put_char_at(item3[s], item3_color, 16 + s, 4); s++; }
           s = 0;
             while (item4[s] != '\0') { put_char_at(item4[s], item4_color, 16 + s, 5); s++; }
	   s = 0;
  	     while (item5[s] != '\0') { put_char_at(item5[s], item5_color, 16 + s, 6); s++; }
	  s = 0;
	     while (item6[s] != '\0') { put_char_at(item6[s], item6_color, 16 + s, 7); s++; }
        }
	   
	    
    }


}




void izvadi_sakuma_tekstu(void) {
    clear_screen();

    draw_menu_bar();
    current_color = 0x0E;
    print("=== KristapsOS kernelis v0.0.4 ===\n");

    current_color = 0x0A;
    print("Startejam kernel ");
    idt_init();
    print("[ GATAVS ]\n\n");
    print("Spied F1, lai ieietu settingos! \n");
    print("Sistema gaida ievadu\n> ");

    // Uzstādām glītu zilo joslu apakšā
    current_color = 0x1F;
    for (int x = 0; x < VGA_WIDTH; x++) put_char_at(' ', current_color, x, 24);
    const char *status = " Statusi: [ IDT: Aktivs ]   [ Tastatura: GAIDA IEVADI ]";
    int s = 0;
    while (status[s] != '\0') {
        put_char_at(status[s], current_color, s + 2, 24);
        s++;
    }

    // Atgriežam krāsu zaļu priekš lietotāja rakstītā teksta
    current_color = 0x0A;


}


// Paštaisīta strncmp funkcija zema līmeņa salīdzināšanai
int mans_strncmp(const char *s1, const char *s2, int n) {
    while (n > 0) {
        if (*s1 != *s2) {
            return *(unsigned char *)s1 - *(unsigned char *)s2;
        }
        if (*s1 == '\0') {
            return 0;
        }
        s1++;
        s2++;
        n--;
    }
    return 0;
}


// Salīdzina pirmos n burtus starp s1 un s2
/*int mans_strncmp(const char *s1, const char *s2, int n) {
    while (n > 0 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}*/

void execute_command(void) {
    // Ja komanda tiek izpildīta, kamēr esam UI logā
    if (ui_mode == 1) {
        // Piespiežam nākamo tekstu rakstīties loga iekšienē!
        // (Pielāgo šos skaitļus X un Y, lai tie precīzi sakrīt ar vietu zem "> " tavam logam)
        cursor_x = 17; 
        cursor_y = 12;
        update_cursor(cursor_x, cursor_y);
    }
 
    if (cmd_index == 0) {
        print("\n> ");
        return;
    }

    cmd_buffer[cmd_index] = '\0';

    // Izveidojam buferus komandai un argumentam
    char komanda[256] = {0};
    char arguments[1024] = {0};
    int i = 0, j = 0;

    // 1. Nolasām pirmo vārdu līdz atstarpei vai galam
    while (cmd_buffer[i] != '\0' && cmd_buffer[i] != ' ' && i < 1023) {
        komanda[i] = cmd_buffer[i];
        i++;
    }
    komanda[i] = '\0';

    // Ja tur ir atstarpe, izlaižam to
    if (cmd_buffer[i] == ' ') {
        i++;
        // 2. Viss pārējais teksts aiziet argumentā
        while (cmd_buffer[i] != '\0' && j < 1023) {
            arguments[j] = cmd_buffer[i];
            i++;
            j++;
        }
    }
    arguments[j] = '\0';

    // === KOMANDU PĀRBAUDES ===
    if (strcmp(komanda, "clear") == 0) {
        clear_screen();
        current_color = 0x0A; // Atgriežam zaļo
        print("> ");
    }
    else if (strcmp(komanda, "about") == 0) {
        print("\n        <--Par manu OS!-->\n\nSi kernel izstradatajs ir Kristaps\nSis ir 64bitu kernelis\nRaksti main, lai atgrieztos sakuma 1945\n> ");
    }
    else if (strcmp(komanda, "main") == 0) {
        izvadi_sakuma_tekstu();
    }
    // JAUNĀ KOMANDA: echo (izvada ekrānā to, ko uzrakstīji)
    else if (strcmp(komanda, "echo") == 0) {
        print("\n");
        if (arguments[0] == '\0') {
            print("Lūdzu, ierakstiet kaut ko pēc 'echo'!");
        } else {
            print(arguments);
        }
        print("\n> ");
    }
    // JAUNĀ KOMANDA: color (maina teksta krāsu)
    else if (strcmp(komanda, "color") == 0) {
        print("\n");
        if (strcmp(arguments, "red") == 0) {
            current_color = 0x0C;
            print("Krasa nomainita uz sarkanu.");
        } else if (strcmp(arguments, "green") == 0) {
            current_color = 0x0A;
            print("Krasa nomainita uz zaļu.");
        } else if (strcmp(arguments, "blue") == 0) {
            current_color = 0x09;
            print("Krasa nomainita uz zilu.");
        } else if (strcmp(arguments, "yellow") == 0) {
            current_color = 0x0E;
            print("Krasa nomainita uz dzeltenu.");
        } else {
            print("Nezinama krasa! Pieejamas: red, green, blue, yellow");
        }
        print("\n> ");
    }else if (strcmp(komanda, "version") == 0) {
        // static const garantē, ka teksts neaizņems vietu uz steka un nepārrakstīs buferus
        print("\n      |-------------------------|\n      |Kernela versija ir v0.0.4|\n      |-------------------------|\n ");
        print("\n");
        print("> ");
    }
    else if (strcmp(komanda, "help") == 0) {
       print("\nPieejamas komandas:\n1. help\n2. clear\n3. version\n4. about\n5. cpu\n6. color\n7. main\n8. ui\n9. disk\n10. notepad\n11. file\n12. cat\n13. testdisk\n14. initfat\n15. ls\n16. rm\n17. desktop\n> ");
    //   print("\n");
    }else if (strcmp(komanda, "cpu") == 0) {
        char vendor[13];
        get_cpu_vendor(vendor);

        char brand[49]; // Vieta 48 zīmēm + \0
        get_cpu_brand(brand);

        static const char* cpu_prefix = "\nProcesora razotajs: ";
        static const char* model_prefix = "Procesora modelis:   ";

        print(cpu_prefix);
        print(vendor);
        print("\n");
        print(model_prefix);
        print(brand);
        print("\n> ");
    }else if (strcmp(komanda, "ui") == 0) {
        // Pārslēdzam visu ekrāna fonu uz patīkamu tumši zilu (0x17 -> zils fons, balts teksts)
        current_color = 0x17; 
        clear_screen();

        // Uzzīmējam smuku pelēku logu ekrāna vidū (0x70 -> pelēks fons, melns teksts)
        // un virsrakstu sarkanu uz pelēka (0x74)
        draw_window(15, 4, 50, 12, 0x70, 0x74, " KristapsOS Sistema ");

        // Izgūstam procesora info, lai parādītu to logā
        char vendor[13];
        get_cpu_vendor(vendor);

        // Ierakstām tekstu tieši mūsu uzzīmētajā logā
        // (Izmantojam put_char_at, lai precīzi kontrolētu pozīcijas)
        const char* msg1 = "UI rezims!";
        const char* msg2 = "CPU:";
        
        int m = 0;
        while(msg1[m] != '\0') { put_char_at(msg1[m], 0x70, 18 + m, 6); m++; }
        m = 0;
        while(msg2[m] != '\0') { put_char_at(msg2[m], 0x70, 18 + m, 8); m++; }
        m = 0;
        while(vendor[m] != '\0') { put_char_at(vendor[m], 0x71, 23 + m, 8); m++; } // zils uz pelēka priekš CPU nosaukuma

        const char* msg_exit = "Raksti 'main', lai atgrieztos.";
        m = 0;
        while(msg_exit[m] != '\0') { put_char_at(msg_exit[m], 0x78, 18 + m, 13); m++; }

        // Novietojam kursoru loga apakšā ievadei
        cursor_x = 18;
        cursor_y = 11;
        current_color = 0x70; // Lietotājs rakstīs ar melniem burtiem uz pelēka fona logā
        print("> ");
    }else if(strcmp(komanda, "1945") == 0){
	print("\nTu atveri slepeno rezimu! \n");
	print("Vesture mode: aktivs\n");
	print("Raksti-talak, lai iesaktu vestures modu\n> ");
}else if(strcmp(komanda, "disk") == 0){
	print("Sakam diska testu...\n");

    uint8_t write_buf[512];
    uint8_t read_buf[512];

    // 1. Sagatavojam testa datus (aizpildām buferi ar kādu tekstu)
    for (int i = 0; i < 512; i++) {
        write_buf[i] = 0;
    }
    // Ierakstām pirmajos baitos kaut ko atpazīstamu
    const char *test_msg = "Teksts diska";
    int len = 0;
    while (test_msg[len] != '\0' && len < 512) {
        write_buf[len] = test_msg[len];
        len++;
    }

    // 2. Mēģinām ierakstīt šo 512 baitu sektoru LBA sektorā Nr. 10
    print("Rakstam sektora 10...\n");
    if (ata_write_sector(10, write_buf) == 0) {
        print("Ierakstisana veiksmiga!\n");
    } else {
        print("KLuDA rakstot!\n");
    }

    // 3. Notīrām lasīšanas buferi drošībai
    for (int i = 0; i < 512; i++) {
        read_buf[i] = 0;
    }

    // 4. Mēģinām nolasīt to pašu sektoru Nr. 10 atpakaļ atmiņā
    print("Lasam sektoru 10...\n");
    if (ata_read_sector(10, read_buf) == 0) {
        print("Nolasits veiksmigi! Saturs:\n");
        // Izdrukājam to, ko nolasījām (uzskatām to par parastu tekstu)
        print((char *)read_buf);
        print("\n");
}else {
        print("KLuDA lasot!\n");
    }
}else if(strcmp(komanda, "notepad") == 0){
ui_mode = 2; // Ieslēdzam redaktora klaviatūras režīmu!

        // === KĀRTĪGS RISINĀJUMS ===
        // Mēs apstiprinām ENTER taustiņu un ieslēdzam interaptus tieši šeit,
        // lai redaktorā varētu normāli saņemt nākamos burtus.
        outb(0x20, 0x20);
        __asm__ volatile("sti");
        // ==========================

        start_editor(); // Šī funkcija strādās tik ilgi, kamēr lietotājs nenospiedīs ESC vai F2

        ui_mode = 0; // Atgriežamies komandrindas režīmā
        izvadi_sakuma_tekstu(); // Atjaunojam sākuma ekrānu, pulksteni un statusus

        }else if(strcmp(komanda, "file") == 0){
	print("\n");
	list_files();

	}else if (strcmp(komanda, "cat") == 0) {
	print("\n");
        // Ja arguments ir tukšs (lietotājs uzrakstīja tikai "cat" bez faila nosaukuma)
        if (arguments[0] == '\0') {
            print("Lietosana: cat <faila_nosaukums>\n> ");
        } else {
            // Izsaucam pareizo FAT16 funkciju un iedodam tai jau gatavo argumentu!
            read_fat16_file(arguments);
            print("> ");
        }
} else if (strcmp(komanda, "testdisk") == 0) {
        testet_disku();

    } else if (strcmp(komanda, "initfat") == 0) {
        init_fat16();

    } else if (strcmp(komanda, "ls") == 0) {
        list_fat16_files();

    }

else if (strcmp(komanda, "rm") == 0) {
        print("\n");
        // Ja arguments ir tukšs (lietotājs uzrakstīja tikai "rm" bez faila)
        if (arguments[0] == '\0') {
            print("Lietosana: rm <faila_nosaukums>\n> ");
        } else {
            // Izsaucam dzēšanu un padodam argumentu (faila nosaukumu)
            if (delete_fat16_file(arguments) == 0) {
                print("Fails veiksmigi izdzests!\n> ");
            } else {
                print("> ");
            }
        }
    }else if (strcmp(komanda, "mkdir") == 0) {
        print("\n");
        if (arguments[0] == '\0') {
            print("Lietosana: mkdir <mapes_nosaukums>\n> ");
        } else {
            if (mkdir_fat16(arguments) == 0) {
                print("Mape veiksmigi izveidota!\n> ");
            } else {
                print("> ");
            }
        }
    }
    else if (strcmp(komanda, "cd") == 0) {
        print("\n");
        if (arguments[0] == '\0') {
            print("Lietosana: cd <mapes_nosaukums>\n> ");
        } else {
            if (change_directory(arguments) == 0) {
                // Lai smuki redzētu, kur esam, var izvadīt ziņu
                print("Nomainita mape. Pasreizējais:  ");
                // (šeit var izdrukāt skaitli, ja tev ir print_int funkcija)
                print("\n> ");
            } else {
                print("> ");
            }
        }
    }else if(strcmp(komanda, "desktop") == 0){
		ui_mode = 3; // Pārslēdzamies uz jauno Darbvirsmas režīmu

        gui_hide_mouse(); // Paslēpjam peli pirms tīrīšanas
        clear_screen();   // Notīrām visu veco konsoles tekstu un sākuma ziņas!

        // Uzzīmējam tīru darbvirsmu
        desktop_init();
        desktop_draw();

        gui_show_mouse(); // Parādām peli virs tīrā galda
        
        cmd_index = 0; // <-- PIEVIENO ŠO, lai konsolei aizmirstas šī komanda
        return;

    return;
     }else {
        // Šis ir pēdējais "else", kas nostrādā TIKAI tad, ja neviena iepriekšējā komanda nesakrita
        print("\nKomanda \"");
        print(komanda);
        print("\" nav atpazita!\n> ");
    }

    cmd_index = 0;
}


//static inline void outb(unsigned short port, unsigned char val) {
  //  __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
//}

// NOLASA datus no porta (Tava jaunā funkcija)
unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}


//void keyboard_handler(void) {
    // 1. Nolasām skankodu no klaviatūras datu porta
  //  unsigned char scancode = inb(0x60);

    // 2. Apstrādājam tikai taustiņa nospiešanu (scancode < 0x80)
    //if (!(scancode & 0x80)) {
        
        // === GLOBĀLIE KONTROLES TAUSTIŅI ===
      //  if (scancode == 0x3B) { // F1 taustiņš ieiet Menu režīmā
        //    ui_mode = 1;
          //  draw_menu_bar();
           // outb(0x20, 0x20);
           // return;
       // }
       // else if (scancode == 0x01) { // ESC taustiņš iziet no Menu režīma
         //   ui_mode = 0;
           // draw_menu_bar();
          //  update_cursor(cursor_x, cursor_y); // Atgriežam kursoru kur tas bija
           // outb(0x20, 0x20);
           // return;
       // }

        // === JA ESAM MENU REŽĪMĀ ===
      //  if (ui_mode == 1) {
        //    if (menu_open == 0) {
          //      if (scancode == 0x4D) { // Bultiņa uz LABO
            //        if (selected_menu < 1) { selected_menu++; draw_menu_bar(); }
              //  }
              //  else if (scancode == 0x4B) { // Bultiņa uz KREISO
                //    if (selected_menu > 0) { selected_menu--; draw_menu_bar(); }
               // }
              //  else if (scancode == 0x1C) { // ENTER (Atveram izvēlni)
                //    menu_open = 1;
                  //  selected_item = 0; // Sākam ar pirmo punktu
                   // draw_menu_bar();
               // }
           // }
            // Ja nolaižamais saraksts JAU IR atvērts (staigājam uz augšu/leju pa punktiem)
          /*  else {
                if (scancode == 0x50) { // Bultiņa uz LEJU
                    if (selected_item < 6) { //--------------------------------------------------------------------------------------------------------------------------------------------------------------
                        selected_item++;
                        draw_menu_bar();
                    }
                }
                else if (scancode == 0x48) { // Bultiņa uz AUGŠU
                    if (selected_item > 0) {
                        selected_item--;
                        draw_menu_bar();
                    }
                }
                else if (scancode == 0x01) { // ESC aizver sarakstu, bet patur mūs augšējā joslā
                    menu_open = 0;
                    draw_menu_bar();
                    outb(0x20, 0x20);
                    return;
                }
                else if (scancode == 0x1C) { // ENTER uz konkrēta punkta (Izpildām darbību!)
                    if (selected_menu == 1 && selected_item == 0) {
                        // Izpildām "Clear Screen"
                        clear_screen();
                        ui_mode = 0; menu_open = 0; // Izejam no UI režīma
                        draw_menu_bar();
                        print("> ");
                    }
                    else if (selected_menu == 1 && selected_item == 1) {
                        // Izpildām "CPU Info"
                        ui_mode = 0; menu_open = 0;
                        draw_menu_bar();
                        // Tā kā 'cpu' komandai tavā execute_command vajag jaunu rindiņu:
                        print("\n");
                        // Šeit mēs varam vienkārši izsaukt cpu info izvadīšanu, bet lai nejauktu kodu,
                        // iedosim komandrindai ziņu, it kā lietotājs būtu uzrakstījis "cpu"
                        cmd_buffer[0] = 'c'; cmd_buffer[1] = 'p'; cmd_buffer[2] = 'u'; cmd_buffer[3] = '\0';
                        cmd_index = 3;
                        execute_command();
                    }else if (selected_menu == 1 && selected_item == 2) {
        // 3. Palidziba
        ui_mode = 0; menu_open = 0;
        draw_menu_bar();
        print("\n");
        // Nobuotējam 'help' komandu tavā buferī
        cmd_buffer[0] = 'h'; cmd_buffer[1] = 'e'; cmd_buffer[2] = 'l'; cmd_buffer[3] = 'p'; cmd_buffer[4] = '\0';
        cmd_index = 4;
        execute_command();
    }else if (selected_menu == 1 && selected_item == 3){
	ui_mode = 0; menu_open = 0;
	draw_menu_bar();
	print("\n");
	cmd_buffer[0] = 'a'; cmd_buffer[1] = 'b'; cmd_buffer[2] = 'o'; cmd_buffer[3] = 'u'; cmd_buffer[4] = 't'; cmd_buffer[5] = '\0';
	cmd_index = 5;
	 execute_command(); 
}else if (selected_menu == 1 && selected_item == 4){
        ui_mode = 0; menu_open = 0;
        draw_menu_bar();
        print("\n");
        cmd_buffer[0] = 'v'; cmd_buffer[1] = 'e'; cmd_buffer[2] = 'r'; cmd_buffer[3] = 's'; cmd_buffer[4] = 'i'; cmd_buffer[5] = 'o'; cmd_buffer[6] = 'n'; cmd_buffer[7] = '\0';
        cmd_index = 7;
         execute_command();
}else if (selected_menu == 1 && selected_item == 5){
        ui_mode = 0; menu_open = 0;
        draw_menu_bar();
        print("\n");
        cmd_buffer[0] = 'u'; cmd_buffer[1] = 'i'; cmd_buffer[2] = '\0';
        cmd_index = 7;
         execute_command();
}
                }
            }
         
        }
        else {
            if (scancode == 0x1C) { // ENTER
                execute_command();
            }
            else if (scancode == 0x0E) { // BACKSPACE
                if (cmd_index > 0) {
                    cmd_index--;
                    cursor_x--;
                    put_char_at(' ', current_color, cursor_x, cursor_y);
                    update_cursor(cursor_x, cursor_y);
                }
            }
            else { // Parasts burts
                char burts = kbd_us[scancode];
                if (burts != 0 && cmd_index < 254) {
                    cmd_buffer[cmd_index] = burts;
                    cmd_index++;
                    char temp[2] = {burts, '\0'};
                    print(temp);
                }
            }
        }
    }

    // 3. OBLIGĀTI: Paziņojam PIC kontrolierim, ka interapts apstrādāts
    outb(0x20, 0x20);
}*/


void atjaunot_pulksteni(void) {
    // 1. Pajautājam CMOS stundas, minūtes, sekundes
    outb(0x70, 0x00); unsigned char sec_bcd = inb(0x71);
    outb(0x70, 0x02); unsigned char min_bcd = inb(0x71);
    outb(0x70, 0x04); unsigned char hour_bcd = inb(0x71);

    // 2. Pārveidojam no BCD uz parastiem skaitļiem
    int sekunde = ((sec_bcd & 0xF0) >> 4) * 10 + (sec_bcd & 0x0F);
    int minute  = ((min_bcd & 0xF0) >> 4) * 10 + (min_bcd & 0x0F);
    int stunda  = ((hour_bcd & 0xF0) >> 4) * 10 + (hour_bcd & 0x0F);

    // Šeit var pieskaņot laika zonu, ja QEMU rāda UTC laiku (piem. stunda = (stunda + 3) % 24;)

    // 3. Pārvēršam skaitļus par ASCII burtis un zīmējam zilajā joslā (Y=24)
    // Pulkstenis sāksies no X=71 līdz X=78 -> "HH:MM:SS"
    char c_color = 0x1F; // Balts uz zila (tavas joslas krāsa)

    put_char_at('0' + (stunda / 10),  c_color, 71, 24);
    put_char_at('0' + (stunda % 10),  c_color, 72, 24);
    put_char_at(':',                  c_color, 73, 24);
    put_char_at('0' + (minute / 10),  c_color, 74, 24);
    put_char_at('0' + (minute % 10),  c_color, 75, 24);
    put_char_at(':',                  c_color, 76, 24);
    put_char_at('0' + (sekunde / 10), c_color, 77, 24);
    put_char_at('0' + (sekunde % 10), c_color, 78, 24);
}


volatile int command_ready = 0; 

void kernel_main(void) {
    idt_init();
    izvadi_sakuma_tekstu();
    init_fat16();

    // 1. Inicializējam peles aparatūru (ŠIS TRŪKA!)
    mouse_init();

    // 2. Inicializējam un uzzīmējam darbvirsmu (Ikonas)

    // 3. Uzzīmējam peli PA VIRSŪ ikonām

    // 4. Tagad ieslēdzam interaptus, kad viss ir uzzīmēts
    __asm__ volatile("sti");

    // 5. Saglabājam sākuma koordinātas salīdzināšanai

while(1) {
    __asm__ volatile("hlt"); // Procesors guļ līdz jebkuram interaptam un lieki netērē CPU

    // === DROŠĪBAS JOSLA: Darbvirsmas režīms ===
    if (ui_mode == 3) {
        // 1. Uzzīmējam darbvirsmu un ikonas
        desktop_draw();

        // 2. LABOTS: Izsaucam viedo peles funkciju.
        // Tā pati sekos līdzi koordinātām, nodzēsīs veco asti un uzzīmēs jauno pozīciju.
        desktop_update_mouse();

        // 3. Pārbaudām klikšķus uz ikonām
        // (Pārbaude 'if (mouse_left_clicked)' jau ir iebūvēta pašā funkcijā, 
        // tāpēc šeit to ārpusē vairs nevajag dublēt)
        check_desktop_clicks();
    }
}


}
