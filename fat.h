#ifndef FAT_H
#define FAT_H

#include <stdint.h>


int write_fat16_file(const char* name, const char* buffer, uint32_t data_len);
int create_fat16_file(const char* name);
int delete_fat16_file(const char* name);

extern int mkdir_fat16(const char* name);
extern int change_directory(const char* name);
extern uint16_t current_dir_cluster;

// FAT16 Boot Sector un BPB struktūra (kopā tieši 512 baiti)
typedef struct {
    uint8_t  jmp[3];
    char     oem_name[8];
    uint16_t bytes_per_sector;      // Parasti 512
    uint8_t  sectors_per_cluster;   // Cik sektoru vienā klasterī
    uint16_t reserved_sectors;      // Sektori pirms FAT (parasti 1 vai vairāk)
    uint8_t  num_fats;              // Parasti 2 FAT tabulas
    uint16_t root_dir_entries;      // Maksimālais failu skaits saknes mapē (piem. 512)
    uint16_t total_sectors_short;   // Kopējais sektoru skaits (ja mazāk par 65535)
    uint8_t  media_type;
    uint16_t sectors_per_fat;       // Cik sektorus aizņem viena FAT tabula
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_large;
    
    // Extended Boot Record (FAT16 specifisks)
    uint8_t  drive_number;
    uint8_t  reserved;
    uint8_t  signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     system_id[8];          // Būs "FAT16   "
    uint8_t  boot_code[448];
    uint16_t boot_signature;        // Jābūt 0xAA55
} __attribute__((packed)) FAT16_BootSector;

// FAT16 Direktorijas ieraksts (katrs fails aizņem 32 baitus)
typedef struct {
    char     filename[8];           // Faila nosaukums (tukšumi beigās)
    char     ext[3];                // Paplašinājums (piem. TXT)
    uint8_t  attributes;            // Faila atribūti (mape, read-only utt.)
    uint8_t  reserved;
    uint8_t  creation_time_ms;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;    // FAT16 gadījumā vienmēr 0
    uint16_t last_write_time;
    uint16_t last_write_date;
    uint16_t first_cluster_low;     // Pirmais klasteris datu apgabalā (kur ir saturs)
    uint32_t file_size;             // Faila izmērs baitos
} __attribute__((packed)) FAT16_DirEntry;

#endif
