#include <stdint.h>

// Ārējā deklarācija no keyboard.c
extern void process_keyboard_scancode(uint8_t scancode);

// Globālie mainīgie, kurus izmanto kernel.c un des
volatile int mouse_x = 0;
volatile int mouse_y = 0;
volatile int mouse_left_clicked = 0;

static uint8_t mouse_cycle = 0;
static uint8_t mouse_byte[3];
static int internal_x = 40 * 8;
static int internal_y = 12 * 8;

// === APARATŪRAS I/O INSTRUKCIJAS (Novērš inb/outb brīdinājumus) ===
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

// === PALĪGFUNKCIJAS PELES INICIALIZĀCIJAI ===
static void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) {
        while (timeout--) {
            if ((inb(0x64) & 1) == 1) return;
        }
    } else {
        while (timeout--) {
            if ((inb(0x64) & 2) == 0) return;
        }
    }
}

static void mouse_write(uint8_t write) {
    mouse_wait(1);
    outb(0x64, 0xD4); // Pasakām kontrolierim, ka sūtīsim komandu pelei
    mouse_wait(1);
    outb(0x60, write);
}

static uint8_t mouse_read(void) {
    mouse_wait(0);
    return inb(0x60);
}

// === PAŠA PELES INICIALIZĀCIJA (Novērš undefined reference kļūdu) ===
void mouse_init(void) {
    uint8_t status;

    // 1. Iespējojam palīgierīces (peles) portu 8042 kontrolierī
    mouse_wait(1);
    outb(0x64, 0xA8);

    // 2. Iespējojam interaptus (IRQ12)
    mouse_wait(1);
    outb(0x64, 0x20); // Komanda: lasīt "Command Byte"
    mouse_wait(0);
    status = (inb(0x60) | 2); // 1. bits iespējo IRQ12 peles interaptu
    status &= ~0x20;          // 5. bits atbloķē peles datu līniju

    mouse_wait(1);
    outb(0x64, 0x60); // Komanda: rakstīt "Command Byte"
    mouse_wait(1);
    outb(0x60, status);

    // 3. Aktivizējam pašu peli (Enable Data Reporting)
    mouse_write(0xF4);
    mouse_read(); // Nolasām un apstiprinām ACK (0xFA) no peles
}

// === PELES DATU APSTRĀDE ===
void process_mouse_byte(uint8_t data) {
    if (mouse_cycle == 0) {
        if (data & 0x08) {
            mouse_byte[0] = data;
            mouse_cycle = 1;
        }
    } else if (mouse_cycle == 1) {
        mouse_byte[1] = data;
        mouse_cycle = 2;
    } else if (mouse_cycle == 2) {
        mouse_byte[2] = data;
        mouse_cycle = 0;

        if (!(mouse_byte[0] & 0x80) && !(mouse_byte[0] & 0x40)) {
            mouse_left_clicked = (mouse_byte[0] & 0x01) ? 1 : 0;

            int x_offset = (int)mouse_byte[1];
            int y_offset = (int)mouse_byte[2];

            if (mouse_byte[0] & 0x10) x_offset |= 0xFFFFFF00;
            if (mouse_byte[0] & 0x20) y_offset |= 0xFFFFFF00;

            internal_x += x_offset;
            internal_y -= y_offset;

            // Ekrāna robežas tekstam 80x25 (pikseļos 640x200)
            if (internal_x < 0) internal_x = 0;
            if (internal_y < 0) internal_y = 0;
            if (internal_x >= 80 * 8) internal_x = (80 * 8) - 1;
            if (internal_y >= 25 * 8) internal_y = (25 * 8) - 1;

            mouse_x = internal_x / 8;
            mouse_y = internal_y / 8;
        }
    }
}

// === APARATŪRAS INTERAPTS (IRQ12) ===
// === APARATŪRAS INTERAPTS (IRQ12) ===
void mouse_handler(void) {
    uint8_t status = inb(0x64);

    // ŠIS IR KRITISKI: Izmantojam 'while', lai iztukšotu visu buferi, 
    // citādi pele un klaviatūra sasals pie pirmā klikšķa!
    while (status & 0x01) {
        uint8_t data = inb(0x60); 

        if (status & 0x20) {
            process_mouse_byte(data);
        } else {
            process_keyboard_scancode(data);
        }
        
        // Nolasām statusu nākamajam cikla aplim
        status = inb(0x64);
    }

    outb(0xA0, 0x20);
    outb(0x20, 0x20);
}
