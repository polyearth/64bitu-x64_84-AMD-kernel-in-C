#include "ata.h"

// Izmantojam jau esošās portu funkcijas no kernel.c
extern void outb(unsigned short port, unsigned char val);
extern unsigned char inb(unsigned short port);

// Tepat lokāli definējam 16-bitu portu lasīšanas un rakstīšanas funkcijas
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

// Palīgfunkcija: Pagaidām, kamēr disks ir aizņemts (Busy) un sagatavojas
static void ata_wait_ready(void) {
    // Gaidām, kamēr BSY (bits 7) pazūd un DRQ (bits 3) kļūst aktīvs vai disks vairs nav aizņemts
    while ((inb(0x1F7) & 0x80) != 0);
    while ((inb(0x1F7) & 0x08) == 0);
}

// Nolasām vienu sektoru (512 baitus)
int ata_read_sector(uint32_t lba, uint8_t *buffer) {
    // 1. Nosūtam diska sagatavošanas komandas (LBA28 režīms)
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F)); // Master drive + LBA augstākie biti
    outb(0x1F1, 0x00);                        // Nulles kļūdu reģistrā
    outb(0x1F2, 1);                           // Lasīsim tieši 1 sektoru
    outb(0x1F3, (uint8_t)lba);                // LBA biti 0-7
    outb(0x1F4, (uint8_t)(lba >> 8));         // LBA biti 8-15
    outb(0x1F5, (uint8_t)(lba >> 16));        // LBA biti 16-23
    
    // 2. Nosūtam komandu 0x20 (Read Sectors)
    outb(0x1F7, 0x20);

    // 3. Gaidām, kamēr disks nolasa datus savā buferī
    ata_wait_ready();

    // 4. Nolasa 256 vārdus (256 * 2 baiti = 512 baiti) no datu porta 0x1F0
    uint16_t *ptr = (uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        ptr[i] = inw(0x1F0);
    }

    return 0; // Viss veiksmīgi!
}

// Ierakstām vienu sektoru (512 baitus)
int ata_write_sector(uint32_t lba, const uint8_t *buffer) {
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F1, 0x00);
    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    
    // Nosūtam komandu 0x30 (Write Sectors)
    outb(0x1F7, 0x30);

    // Gaidām, kad disks gatavs saņemt datus ierakstīšanai
    ata_wait_ready();

    // Nosūtam 256 vārdus (512 baitus) uz portu 0x1F0
    const uint16_t *ptr = (const uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        outw(0x1F0, ptr[i]);
    }

    // Liekam diskam pabeigt rakstīšanu (Flush Cache)
    outb(0x1F7, 0xE7);
    while ((inb(0x1F7) & 0x80) != 0); // Pagaidām, kamēr pabeidz

    return 0;
}
