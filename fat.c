#include "fat.h"
uint16_t find_free_cluster(void);
extern int strcmp(const char *s1, const char *s2);

int mans_strcasecmp(const char *s1, const char *s2);
// Izmantojam jau gatavo ATA sektoru lasītāju
extern int ata_read_sector(uint32_t lba, uint8_t *buffer);
extern void print(const char* str);

uint16_t current_dir_cluster = 0; // 0 nozīmē Root (saknes) mapi
uint32_t data_start_lba = 0;      // Šo vajag inicializēt init_fat16 funkcijā (parasti root_dir_lba + 4 vai 14)
// Globālie mainīgie, lai mums nebūtu katru reizi jārēķina no jauna
uint32_t fat_lba = 0;          // Kur sākas pirmā FAT tabula
uint32_t root_dir_lba = 0;     // Kur sākas saknes mape (failu saraksts)
uint32_t data_lba = 0;         // Kur sākas paši failu dati (klasteris 2)
uint32_t sectors_per_cluster = 0;


// Atgriež sektoru, kurā sākas mape, un cik sektorus tā aizņem
uint32_t get_dir_lba(uint16_t cluster, uint32_t* sectors_count) {
    if (cluster == 0) {
        // Standarta FAT16 saknes mape (512 ieraksti * 32 baiti / 512 baiti sektorā) ir 32 sektori.
        // Ja tavā init_fat16 izvada citu skaitu, vari norādīt to, bet 32 ir drošs standarts.
        *sectors_count = 32; 
        return root_dir_lba;
    } else {
        // Izmantojam dinamisko vērtību no tava bpb!
        *sectors_count = sectors_per_cluster; 
        return data_lba + (cluster - 2) * sectors_per_cluster; 
    }
}


// Salīdzina divus stringus, ignorējot lielos un mazos burtus
int mans_strcasecmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        char c1 = *s1;
        char c2 = *s2;

        // Ja burts ir lielais (A-Z), pārvēršam to par mazo (a-z)
        if (c1 >= 'A' && c1 <= 'Z') c1 = c1 + 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 = c2 + 32;

        if (c1 != c2) {
            return (unsigned char)c1 - (unsigned char)c2;
        }
        s1++;
        s2++;
    }

    char c1 = *s1;
    char c2 = *s2;
    if (c1 >= 'A' && c1 <= 'Z') c1 = c1 + 32;
    if (c2 >= 'A' && c2 <= 'Z') c2 = c2 + 32;

    return (unsigned char)c1 - (unsigned char)c2;
}

// Funkcija, kas inicializē FAT16 un izvada diska info
void init_fat16(void) {
    uint8_t sector_buf[512];

    // 1. Nolasām pašu pirmo sektoru (LBA 0)
    if (ata_read_sector(0, sector_buf) != 0) {
        print("FAT16 init kluda: Nevar nolasit LBA 0!\n");
        return;
    }

    // Pārvēršam jēlos baitus par strukturētu Boot Sector
    FAT16_BootSector* bpb = (FAT16_BootSector*)sector_buf;

    // Pārbaudām, vai tas vispār ir FAT16 disks (jābūt parakstam 0xAA55 beigās)
    if (bpb->boot_signature != 0xAA55) {
        print("FAT16 init kluda: Nav derigs FAT paraksts!\n");
        return;
    }

    sectors_per_cluster = bpb->sectors_per_cluster;

    // 2. Veicam matemātiku, lai atrastu svarīgākos sektorus
    // FAT tabula sākas uzreiz pēc rezervētajiem sektoriem (parasti 1. sektors)
    fat_lba = bpb->reserved_sectors;

    // Saknes mape (Root Directory) sākas aiz visām FAT tabulām
    root_dir_lba = fat_lba + (bpb->num_fats * bpb->sectors_per_fat);

    // Aprēķinām, cik sektorus aizņem pati saknes mape (katrs fails aizņem 32 baitus)
    uint32_t root_dir_sectors = ((bpb->root_dir_entries * 32) + (bpb->bytes_per_sector - 1)) / bpb->bytes_per_sector;

    // Datu apgabals sākas uzreiz aiz saknes mapes
    data_lba = root_dir_lba + root_dir_sectors;

    // 3. Izvadām smuku info uz ekrāna
    print("\n--- FAT16 SISTEMA ATRASTA ---\n");
    print("OEM nosaukums: ");

    // OEM nosaukums nav null-terminated, tāpēc izvadām pa burtam
    char oem[9] = {0};
    for(int i = 0; i < 8; i++) oem[i] = bpb->oem_name[i];
    print(oem);
    print("\n");

    print("Sektori klasteri: ");
    char cluster_str[4] = { '0' + sectors_per_cluster, '\n', '\0' };
    print(cluster_str);

    print("Maks. failu skaits mape: ");
    if (bpb->root_dir_entries == 512) {
        print("512\n");
    } else {
        print("Atbilst standartam\n");
    }
    print("-----------------------------\n> ");
}

// Funkcija, kas nolasa un izvada visus failus no FAT16 saknes mapes
void list_fat16_files(void) {
	if (root_dir_lba == 0) {
        print("Kluda: FAT16 sistema nav inicializeta!\n");
        return;
    }
    uint32_t sectors_count = 0;
    uint32_t dir_start_lba = get_dir_lba(current_dir_cluster, &sectors_count);

    uint8_t sector_buf[512];
    FAT16_DirEntry* entry = 0;
    int ierakstu_skaits = 0;

    print("\n--- FAILI UN MAPES UZ DISKA ---\n");

    for (uint32_t sector_offset = 0; sector_offset < sectors_count; sector_offset++) {
        uint32_t current_sector_lba = dir_start_lba + sector_offset;
        if (ata_read_sector(current_sector_lba, sector_buf) != 0) return;

        for (int i = 0; i < 16; i++) {
            entry = (FAT16_DirEntry*)&sector_buf[i * 32];

            // Pārbaudes
            if (entry->filename[0] == 0x00) break; // Ja sasniegtas beigas, pārtraucam lasīšanu
            if ((uint8_t)entry->filename[0] == 0xE5) continue; // Izdzēsts ieraksts
            if (entry->attributes & 0x08) continue; // Sistēmas Volume Label, ignorējam

            ierakstu_skaits++;
	if (current_dir_cluster == 0) {
    if (entry->filename[0] == '.') continue; // Izlaižam '.' un '..' saknē
}
            // Pārbaudām, vai tā ir mape vai fails un izvadām tipu
            if (entry->attributes & 0x10) {
                print("[DIR]  ");
            } else {
                print("[FILE] ");
            }

            // Izvadām nosaukumu (8 zīmes)
            char f_name[9] = {0};
            int k = 0;
            for(int j = 0; j < 8; j++) {
                if(entry->filename[j] != ' ') f_name[k++] = entry->filename[j];
            }
            print(f_name);

            // Ja tas ir fails un tam ir paplašinājums, izvadām to
            if (!(entry->attributes & 0x10) && entry->ext[0] != ' ') {
                print(".");
                char f_ext[4] = {0};
                int e_k = 0;
                for(int j = 0; j < 3; j++) {
                    if(entry->ext[j] != ' ') f_ext[e_k++] = entry->ext[j];
                }
                print(f_ext);
            }
            print("\n");
        }
    }

    if (ierakstu_skaits == 0) {
        print("(Pašreizējā mape ir tukša)\n");
    }
    print("-------------------------------\n");
}
// Funkcija, kas atrod failu pēc nosaukuma un izprintē tā saturu
void read_fat16_file(const char* name) {
    if (root_dir_lba == 0) {
        print("Kluda: FAT16 nav inicializets! Palaid 'initfat' vispirms.\n");
        return;
    }

    // 1. Attīrām ievades faila nosaukumu no Enter (\r, \n) un atstarpēm gala pozīcijās

uint32_t sectors_count = 0;
    uint32_t dir_start_lba = get_dir_lba(current_dir_cluster, &sectors_count);
    char clean_name[13];
    int cn_idx = 0;
    while (name[cn_idx] != '\0' && name[cn_idx] != '\r' && name[cn_idx] != '\n' && name[cn_idx] != ' ' && cn_idx < 12) {
        clean_name[cn_idx] = name[cn_idx];
        cn_idx++;
    }
    clean_name[cn_idx] = '\0';


    uint8_t sector_buf[512];
    FAT16_DirEntry* entry;

    // Lasām saknes mapi
for (uint32_t sector_offset = 0; sector_offset < sectors_count; sector_offset++) {
        uint32_t current_sector_lba = dir_start_lba + sector_offset;
        if (ata_read_sector(current_sector_lba, sector_buf) != 0) return;
        for (int i = 0; i < 16; i++) {
            entry = (FAT16_DirEntry*)&sector_buf[i * 32];

            if ((uint8_t)entry->filename[0] == 0x00) continue; // Tukšs
            if ((uint8_t)entry->filename[0] == 0xE5) continue; // Izdzēsts
            if (entry->attributes & 0x08) continue;   // Voluma etiķete

            // Izveidojam normālu lasāmu nosaukumu "VĀRDS.PAP" no FAT16 ieraksta struktūras
            char disk_name[13];
            int k = 0;
            for(int j = 0; j < 8; j++) {
                if(entry->filename[j] != ' ') disk_name[k++] = entry->filename[j];
            }
            
            int has_ext = 0;
            char ext_buf[4];
            int ext_idx = 0;
            for(int j = 0; j < 3; j++) {
                if(entry->ext[j] != ' ') {
                    ext_buf[ext_idx++] = entry->ext[j];
                    has_ext = 1;
                }
            }
            ext_buf[ext_idx] = '\0';

            if (has_ext) {
                disk_name[k++] = '.';
                for(int j = 0; j < ext_idx; j++) disk_name[k++] = ext_buf[j];
            }
            disk_name[k] = '\0';

            print("\n");

            // Salīdzinām ar reģistru nejutīgo funkciju
            if (mans_strcasecmp(clean_name, disk_name) == 0) {

                // Paņemam sākuma klastera numuru no FAT struktūras (drošā veidā pēc offset 26 baitu pozīcijas)
                uint16_t cluster = *(uint16_t*)((uint8_t*)entry + 26);
                uint32_t bytes_left = entry->file_size;
                uint8_t data_buf[512];

                if (bytes_left == 0) {
                    print("(Fails ir tukšs)\n");
                    return;
                }

                // Lasām klasterus vienu pēc otra, kamēr fails beidzas
                while (bytes_left > 0 && cluster >= 2 && cluster < 0xFFF8) {
                    uint32_t cluster_lba = data_lba + (cluster - 2) * sectors_per_cluster;

                    for (uint32_t s = 0; s < sectors_per_cluster && bytes_left > 0; s++) {
                        if (ata_read_sector(cluster_lba + s, data_buf) != 0) {
                            print("Kluda lasot faila datus!\n");
                            return;
                        }

                        uint32_t to_print = (bytes_left > 512) ? 512 : bytes_left;
                        for (uint32_t b = 0; b < to_print; b++) {
                            char c_str[2] = { (char)data_buf[b], '\0' };
                            print(c_str);
                        }
                        bytes_left -= to_print;
                    }

                    // Ja vēl palikuši dati, skatāmies nākamo klasteri FAT tabulā (2 baiti uz ierakstu)
                    if (bytes_left > 0) {
                        uint32_t fat_sector = fat_lba + (cluster * 2) / 512;
                        uint32_t fat_offset = (cluster * 2) % 512;
                        uint8_t fat_buf[512];

                        if (ata_read_sector(fat_sector, fat_buf) != 0) {
                            print("Kluda lasot FAT tabulu!\n");
                            return;
                        }
                        cluster = *(uint16_t*)&fat_buf[fat_offset];
                    }
                }
                print("\n");
                return;
            }
        }
    }
    print("Kluda: Fails netika atrasts!\n");
}







// Deklarējam ATA funkcijas, lai fat.c tās pazīst
extern int ata_read_sector(uint32_t lba, uint8_t *buffer);
extern int ata_write_sector(uint32_t lba, const uint8_t *buffer);





// =========================================================================
// JAUNA PALĪGFUNKCIJA: Atrod apakšmapes klasteri konkrētā direktorijā pēc vārda
// =========================================================================
uint16_t find_dir_cluster(uint16_t start_cluster, const char* dir_name) {
    uint32_t sectors_count;
    uint32_t dir_lba = get_dir_lba(start_cluster, &sectors_count);
    uint8_t sector_buf[512];
    FAT16_DirEntry* entry;

    char clean_target[9];
    int idx = 0;
    while (dir_name[idx] != '\0' && dir_name[idx] != '/' && idx < 8) {
        char c = dir_name[idx];
        if (c >= 'a' && c <= 'z') c -= 32; // Uz lielajiem burtiem
        clean_target[idx] = c;
        idx++;
    }
    clean_target[idx] = '\0';

    for (uint32_t s = 0; s < sectors_count; s++) {
        if (ata_read_sector(dir_lba + s, sector_buf) != 0) return 0xFFFF;
        for (int i = 0; i < 16; i++) {
            entry = (FAT16_DirEntry*)&sector_buf[i * 32];
            if (entry->filename[0] == 0x00) break;
            if ((uint8_t)entry->filename[0] == 0xE5) continue;
            if (!(entry->attributes & 0x10)) continue; // Ja tā nav mape, izlaižam

            char disk_name[9];
            int k = 0;
            for (int j = 0; j < 8; j++) {
                if (entry->filename[j] != ' ') disk_name[k++] = entry->filename[j];
            }
            disk_name[k] = '\0';

            if (mans_strcasecmp(clean_target, disk_name) == 0) {
                return entry->first_cluster_low;
            }
        }
    }
    return 0xFFFF; // Mape netika atrasta
}

// =========================================================================
// ATJAUNINĀTĀ FUNKCIJA: Atbalsta ceļus formātā MAPE1/FAILS.TXT
// =========================================================================
// =========================================================================
// JAUNA FUNKCIJA: Nolasa failu un saglabā to RAM buferī (neprintē uz ekrāna)
// =========================================================================
int load_fat16_file(const char* name, char* out_buffer, uint32_t max_len) {
    uint16_t target_dir_cluster = current_dir_cluster;
    const char* final_file_name = name;

    // Apstrādājam ceļu, ja faila nosaukumā ir slīpsvītras (piem. MAPE1/FAILS.TXT)
    int last_slash_idx = -1;
    for (int i = 0; name[i] != '\0'; i++) {
        if (name[i] == '/') last_slash_idx = i;
    }
    if (last_slash_idx != -1) {
        char comp[12];
        int c_len = 0;
        for (int m = 0; m < last_slash_idx && c_len < 11; m++) {
            if (name[m] == '/') {
                comp[c_len] = '\0';
                if (c_len > 0) {
                    uint16_t next = find_dir_cluster(target_dir_cluster, comp);
                    if (next != 0xFFFF) target_dir_cluster = next;
                }
                c_len = 0;
            } else {
                comp[c_len++] = name[m];
            }
        }
        comp[c_len] = '\0';
        if (c_len > 0) {
            uint16_t next = find_dir_cluster(target_dir_cluster, comp);
            if (next != 0xFFFF) target_dir_cluster = next;
        }
        final_file_name = &name[last_slash_idx + 1];
    }

    // Sagatavojam salīdzināmo nosaukumu 8.3 formātā
    char clean_name[13];
    int cn_idx = 0;
    while (final_file_name[cn_idx] != '\0' && final_file_name[cn_idx] != ' ' && cn_idx < 12) {
        clean_name[cn_idx] = final_file_name[cn_idx];
        cn_idx++;
    }
    clean_name[cn_idx] = '\0';

    uint32_t sectors_count = 0;
    uint32_t dir_start_lba = get_dir_lba(target_dir_cluster, &sectors_count);
    uint8_t sector_buf[512];
    FAT16_DirEntry* entry;

    for (uint32_t sector_offset = 0; sector_offset < sectors_count; sector_offset++) {
        if (ata_read_sector(dir_start_lba + sector_offset, sector_buf) != 0) return -1;
        for (int i = 0; i < 16; i++) {
            entry = (FAT16_DirEntry*)&sector_buf[i * 32];
            if ((uint8_t)entry->filename[0] == 0x00) break;
            if ((uint8_t)entry->filename[0] == 0xE5 || (entry->attributes & 0x08)) continue;

            char disk_name[13];
            int k = 0;
            for(int j = 0; j < 8; j++) if(entry->filename[j] != ' ') disk_name[k++] = entry->filename[j];
            int has_ext = 0;
            char ext_buf[4]; int ext_idx = 0;
            for(int j = 0; j < 3; j++) if(entry->ext[j] != ' ') { ext_buf[ext_idx++] = entry->ext[j]; has_ext = 1; }
            ext_buf[ext_idx] = '\0';
            if (has_ext) {
                disk_name[k++] = '.';
                for(int j = 0; j < ext_idx; j++) disk_name[k++] = ext_buf[j];
            }
            disk_name[k] = '\0';

            // Ja fails atrasts, ielādējam to buferī
            if (mans_strcasecmp(clean_name, disk_name) == 0) {
                uint16_t cluster = entry->first_cluster_low;
                uint32_t bytes_to_read = (entry->file_size > max_len) ? max_len : entry->file_size;
                uint32_t bytes_left = bytes_to_read;
                uint32_t buf_idx = 0;
                uint8_t data_buf[512];

                while (bytes_left > 0 && cluster >= 2 && cluster < 0xFFF8) {
                    uint32_t cluster_lba = data_lba + (cluster - 2) * sectors_per_cluster;
                    for (uint32_t s = 0; s < sectors_per_cluster && bytes_left > 0; s++) {
                        if (ata_read_sector(cluster_lba + s, data_buf) != 0) return -1;
                        uint32_t chunk = (bytes_left > 512) ? 512 : bytes_left;
                        for (uint32_t b = 0; b < chunk; b++) {
                            out_buffer[buf_idx++] = (char)data_buf[b];
                        }
                        bytes_left -= chunk;
                    }
                    if (bytes_left > 0) {
                        uint32_t fat_sector = fat_lba + (cluster * 2) / 512;
                        uint32_t fat_offset = (cluster * 2) % 512;
                        if (ata_read_sector(fat_sector, data_buf) != 0) return -1;
                        cluster = *(uint16_t*)&data_buf[fat_offset];
                    }
                }
                out_buffer[buf_idx] = '\0'; // Noslēdzam stringu
                return buf_idx; // Atgriežam nolasīto baitu skaitu
            }
        }
    }
    return -1; // Fails netika atrasts
}

// =========================================================================
// UZLABOTA FUNKCIJA: Atbalsta ceļus un failu PĀRRAKSTĪŠANU (Overwrite)
// =========================================================================
int write_fat16_file(const char* name, const char* buffer, uint32_t data_len) {
    uint16_t target_dir_cluster = current_dir_cluster;
    const char* final_file_name = name;

    // Ceļa sadalīšana slīpsvītru gadījumā
    int last_slash_idx = -1;
    for (int i = 0; name[i] != '\0'; i++) {
        if (name[i] == '/') last_slash_idx = i;
    }
    if (last_slash_idx != -1) {
        char dir_part[64];
        int i = 0;
        for (; i < last_slash_idx && i < 63; i++) dir_part[i] = name[i];
        dir_part[i] = '\0';
        final_file_name = &name[last_slash_idx + 1];

        int start = 0;
        for (int j = 0; j <= i; j++) {
            if (dir_part[j] == '/' || dir_part[j] == '\0') {
                char comp[12]; int c_len = 0;
                for (int m = start; m < j && c_len < 11; m++) comp[c_len++] = dir_part[m];
                comp[c_len] = '\0';
                if (c_len > 0) {
                    uint16_t next = find_dir_cluster(target_dir_cluster, comp);
                    if (next == 0xFFFF) return -1; // Mape neeksistē
                    target_dir_cluster = next;
                }
                start = j + 1;
            }
        }
    }

    // Izveidojam 8.3 formātu salīdzināšanai un saglabāšanai
    char fat_name[8]; char fat_ext[3];
    for(int j=0; j<8; j++) fat_name[j] = ' ';
    for(int j=0; j<3; j++) fat_ext[j] = ' ';
    int l_idx = 0;
    while(final_file_name[l_idx] != '\0' && final_file_name[l_idx] != '.' && l_idx < 8) {
        char c = final_file_name[l_idx];
        if(c >= 'a' && c <= 'z') c -= 32;
        fat_name[l_idx] = c;
        l_idx++;
    }
    if (final_file_name[l_idx] == '.') {
        l_idx++; int e_idx = 0;
        while(final_file_name[l_idx] != '\0' && e_idx < 3) {
            char c = final_file_name[l_idx];
            if(c >= 'a' && c <= 'z') c -= 32;
            fat_ext[e_idx] = c;
            l_idx++; e_idx++;
        }
    }

    uint32_t sectors_count = 0;
    uint32_t dir_start_lba = get_dir_lba(target_dir_cluster, &sectors_count);
    uint8_t sector_buf[512];
    FAT16_DirEntry* entry = 0;
    uint32_t entry_lba = 0;
    int entry_index = -1;
    uint16_t file_cluster = 0;

    // MEKLĒJAM: Vai šāds fails jau eksistē? (Lai pārrakstītu) vai meklējam brīvu slotu
    int existing_found = 0;
    for (uint32_t s = 0; s < sectors_count; s++) {
        if (ata_read_sector(dir_start_lba + s, sector_buf) != 0) return -1;
        for (int i = 0; i < 16; i++) {
            FAT16_DirEntry* e = (FAT16_DirEntry*)&sector_buf[i * 32];
            if (e->filename[0] != 0x00 && (uint8_t)e->filename[0] != 0xE5 && !(e->attributes & 0x10)) {
                int match = 1;
                for(int j=0; j<8; j++) if(e->filename[j] != fat_name[j]) match = 0;
                for(int j=0; j<3; j++) if(e->ext[j] != fat_ext[j]) match = 0;
                if (match) {
                    entry_lba = dir_start_lba + s;
                    entry_index = i;
                    file_cluster = e->first_cluster_low;
                    existing_found = 1;
                    break;
                }
            }
            if ((e->filename[0] == 0x00 || (uint8_t)e->filename[0] == 0xE5) && entry_index == -1) {
                entry_lba = dir_start_lba + s;
                entry_index = i;
            }
        }
        if (existing_found) break;
    }

    if (entry_index == -1) return -1; // Mape pilna

    // Ja fails ir jauns, piešķiram brīvu klasteri
    if (!existing_found) {
        file_cluster = find_free_cluster();
        if (file_cluster == 0) return -1;
    }

    // Aizpildām/Atjaunojam ierakstu direktorijā
    if (ata_read_sector(entry_lba, sector_buf) != 0) return -1;
    entry = (FAT16_DirEntry*)&sector_buf[entry_index * 32];
    
    for(int j=0; j<8; j++) entry->filename[j] = fat_name[j];
    for(int j=0; j<3; j++) entry->ext[j] = fat_ext[j];
    entry->attributes = 0x00;
    entry->first_cluster_low = file_cluster;
    entry->file_size = data_len;
    if (ata_write_sector(entry_lba, sector_buf) != 0) return -1;

    // Ierakstām datus diska klasterī
    uint32_t file_data_lba = data_lba + (file_cluster - 2) * sectors_per_cluster;
    uint8_t data_buf[512] = {0};
    for (uint32_t m = 0; m < data_len && m < 512; m++) data_buf[m] = buffer[m];
    if (ata_write_sector(file_data_lba, data_buf) != 0) return -1;

    // Atjaunojam FAT tabulu (tikai ja tas bija jauns fails)
    if (!existing_found) {
        if (ata_read_sector(fat_lba, sector_buf) != 0) return -1;
        uint16_t* fat = (uint16_t*)sector_buf;
        fat[file_cluster] = 0xFFFF;
        if (ata_write_sector(fat_lba, sector_buf) != 0) return -1;
    }

    return 0;
}







// Funkcija, kas atrod pirmo pilnībā brīvo klasteri FAT tabulā
uint16_t find_free_cluster(void) {
    uint8_t fat_buf[512];
    // FAT16 tabula sākas no fat_lba. Skenējam sektorus pēc kārtas.
    // Parasti FAT16 tabulā ir vairāki desmiti vai simti sektoru.
    for (uint32_t s = 0; s < 32; s++) { // Skenējam pirmos 32 FAT tabulas sektorus
        if (ata_read_sector(fat_lba + s, fat_buf) != 0) return 0;
        
        uint16_t* fat_table = (uint16_t*)fat_buf;
        for (int i = 0; i < 256; i++) {
            uint32_t entry_idx = s * 256 + i;
            // Pirmie divi FAT ieraksti (0 un 1) ir rezervēti sistēmai
            if (entry_idx < 2) continue;
            
            if (fat_table[i] == 0x0000) {
                return (uint16_t)entry_idx; // Atrasts brīvs klasteris!
            }
        }
    }
    return 0; // Nav brīvu klasteru
}

// Funkcija, kas ieraksta FAT tabulā klastera vērtību
int write_fat_entry(uint16_t cluster, uint16_t value) {
    uint32_t fat_sector = fat_lba + (cluster * 2) / 512;
    uint32_t fat_offset = (cluster * 2) % 512;
    uint8_t fat_buf[512];

    if (ata_read_sector(fat_sector, fat_buf) != 0) return -1;
    *(uint16_t*)&fat_buf[fat_offset] = value;
    if (ata_write_sector(fat_sector, fat_buf) != 0) return -1;

    // Tā kā FAT16 parasti ir 2 FAT tabulas (dublēšanai), ierakstām arī otrajā tabulā
    // sectors_per_fat parasti tiek nolasīts boot sektorā (mūsu gadījumā pieņemsim nobīdi)
    // Ja tev ir boot_sector mainīgais, vari izmantot precīzu izmēru. 
    // Šis solis nav obligāts QEMU darbībai, bet drošībai var izmantot:
    return 0;
}

// Galvenā funkcija: Izveido pilnīgi jaunu failu diskā
int create_fat16_file(const char* name) {
    if (root_dir_lba == 0) return -1;

    // 1. Apstrādājam nosaukumu (jāpārvērš uz 8.3 FAT formātu)
    char clean_name[13];
    int cn_idx = 0;
    while (name[cn_idx] != '\0' && name[cn_idx] != ' ' && cn_idx < 12) {
        clean_name[cn_idx] = name[cn_idx];
        cn_idx++;
    }
    clean_name[cn_idx] = '\0';

    // Sadalām nosaukumu pamatdaļā (8 baiti) un paplašinājumā (3 baiti)
    char fat_name[8];
    char fat_ext[3];
    for (int i = 0; i < 8; i++) fat_name[i] = ' ';
    for (int i = 0; i < 3; i++) fat_ext[i] = ' ';

    int dot_pos = -1;
    for (int i = 0; clean_name[i] != '\0'; i++) {
        if (clean_name[i] == '.') { dot_pos = i; break; }
    }

    if (dot_pos != -1) {
        for (int i = 0; i < dot_pos && i < 8; i++) fat_name[i] = clean_name[i];
        for (int i = 0; i < 3 && clean_name[dot_pos + 1 + i] != '\0'; i++) {
            fat_ext[i] = clean_name[dot_pos + 1 + i];
        }
    } else {
        for (int i = 0; clean_name[i] != '\0' && i < 8; i++) fat_name[i] = clean_name[i];
    }

    // 2. Skenējam saknes mapi, lai pārliecinātos, ka fails jau neeksistē, 
    // un reizē atrastu pirmo brīvo ieraksta slotu (kur pirmais baits ir 0x00 vai 0xE5)
uint8_t sector_buf[512];
    uint32_t free_entry_sector = 0;
    int free_entry_index = -1;
    
    // Iegūstam pareizos adreses datus pašreizējai mapei
    uint32_t sectors_count = 0;
    uint32_t dir_start_lba = get_dir_lba(current_dir_cluster, &sectors_count);

    for (uint32_t sector_offset = 0; sector_offset < sectors_count; sector_offset++) {
        uint32_t current_root_lba = dir_start_lba + sector_offset;
        if (ata_read_sector(current_root_lba, sector_buf) != 0) return -1;
        for (int i = 0; i < 16; i++) {
            FAT16_DirEntry* entry = (FAT16_DirEntry*)&sector_buf[i * 32];
            uint8_t first_char = (uint8_t)entry->filename[0];

            // Atrodam pirmo brīvo slotu un piefiksējam to
            if ((first_char == 0x00 || first_char == 0xE5) && free_entry_index == -1) {
                free_entry_sector = current_root_lba;
                free_entry_index = i;
            }

            // Ja fails ar tādu nosaukumu jau eksistē, jaunu taisīt nevajag!
            if (first_char != 0x00 && first_char != 0xE5) {
                int match = 1;
                for (int j = 0; j < 8; j++) if (entry->filename[j] != fat_name[j]) match = 0;
                for (int j = 0; j < 3; j++) if (entry->ext[j] != fat_ext[j]) match = 0;
                if (match) return 0; // Fails jau eksistē, viss kārtībā!
            }
        }
    }

    if (free_entry_index == -1) {
        print("Kluda: Saknes mape ir pilna!\n");
        return -1;
    }

    // 3. Atrodam brīvu klasteri jaunajam failam
    uint16_t free_cluster = find_free_cluster();
    if (free_cluster == 0) {
        print("Kluda: Nav brivu klasteru diska!\n");
        return -1;
    }

    // Rezervējam klasteri FAT tabulā, ierakstot tur 0xFFFF (faila beigas)
    if (write_fat_entry(free_cluster, 0xFFFF) != 0) return -1;

    // 4. Ierakstām jauno failu atrastajā saknes mapes slotā
    if (ata_read_sector(free_entry_sector, sector_buf) != 0) return -1;
    FAT16_DirEntry* new_entry = (FAT16_DirEntry*)&sector_buf[free_entry_index * 32];

    // Aizpildām struktūru
    for (int i = 0; i < 8; i++) new_entry->filename[i] = fat_name[i];
    for (int i = 0; i < 3; i++) new_entry->ext[i] = fat_ext[i];
    new_entry->attributes = 0x00; // Parasts fails
    new_entry->reserved = 0;
    new_entry->creation_time_ms = 0;
    new_entry->creation_time = 0;
    new_entry->creation_date = 0;
    new_entry->last_access_date = 0;
    new_entry->first_cluster_high = 0; // FAT16 vienmēr 0
    new_entry->last_write_time = 0;
    new_entry->last_write_date = 0;
    new_entry->first_cluster_low = free_cluster; // Norādām tikko atrasto klasteri
    new_entry->file_size = 0; // Sākumā izmērs ir 0

    // Saglabājam atjaunināto saknes mapes sektoru diskā
    if (ata_write_sector(free_entry_sector, sector_buf) != 0) {
        print("Kluda: Neizdevas ierakstit jauno failu direktorija!\n");
        return -1;
    }

    return 0; // Fails veiksmīgi izveidots!
}




int delete_fat16_file(const char* name) {
    if (root_dir_lba == 0) return -1;

    // Attīrām ievades nosaukumu
    char clean_name[13];
    int cn_idx = 0;
    while (name[cn_idx] != '\0' && name[cn_idx] != '\r' && name[cn_idx] != '\n' && name[cn_idx] != ' ' && cn_idx < 12) {
        clean_name[cn_idx] = name[cn_idx];
        cn_idx++;
    }
    clean_name[cn_idx] = '\0';

    uint8_t sector_buf[512];
    FAT16_DirEntry* entry;

    // Meklējam failu saknes mapē
// Meklējam failu pašreizējā mapē
    uint32_t sectors_count = 0;
    uint32_t dir_start_lba = get_dir_lba(current_dir_cluster, &sectors_count);

    for (uint32_t sector_offset = 0; sector_offset < sectors_count; sector_offset++) {
        uint32_t current_root_lba = dir_start_lba + sector_offset;
        if (ata_read_sector(current_root_lba, sector_buf) != 0) return -1;

        for (int i = 0; i < 16; i++) {
            entry = (FAT16_DirEntry*)&sector_buf[i * 32];

            // Ignorējam jau tukšos vai izdzēstos ierakstus
            if ((uint8_t)entry->filename[0] == 0x00 || (uint8_t)entry->filename[0] == 0xE5) continue;
            if (entry->attributes & 0x08) continue; // Ignorējam Volume Label

            // Uzbūvējam salīdzināmo nosaukumu (8.3 formāts)
            char disk_name[13];
            int k = 0;
            for(int j = 0; j < 8; j++) if(entry->filename[j] != ' ') disk_name[k++] = entry->filename[j];
            
            int has_ext = 0;
            char ext_buf[4];
            int ext_idx = 0;
            for(int j = 0; j < 3; j++) {
                if(entry->ext[j] != ' ') { ext_buf[ext_idx++] = entry->ext[j]; has_ext = 1; }
            }
            ext_buf[ext_idx] = '\0';
            if (has_ext) {
                disk_name[k++] = '.';
                for(int j = 0; j < ext_idx; j++) disk_name[k++] = ext_buf[j];
            }
            disk_name[k] = '\0';

            // Ja fails atrasts, dzēšam to!
            if (mans_strcasecmp(clean_name, disk_name) == 0) {
                uint16_t cluster = entry->first_cluster_low;

                // 1. Atbrīvojam visus saistītos klasterus FAT tabulā
                while (cluster >= 2 && cluster < 0xFFF8) {
                    uint32_t fat_sector = fat_lba + (cluster * 2) / 512;
                    uint32_t fat_offset = (cluster * 2) % 512;
                    uint8_t fat_buf[512];

                    if (ata_read_sector(fat_sector, fat_buf) != 0) return -1;
                    
                    uint16_t next_cluster = *(uint16_t*)&fat_buf[fat_offset];
                    
                    // Ierakstām 0x0000 (atzīmējam klasteri kā brīvu)
                    *(uint16_t*)&fat_buf[fat_offset] = 0x0000;
                    if (ata_write_sector(fat_sector, fat_buf) != 0) return -1;

                    cluster = next_cluster;
                }

                // 2. Atzīmējam failu kā izdzēstu direktorijas ierakstā (ierakstām 0xE5 kā pirmo burtu)
                entry->filename[0] = 0xE5;
                if (ata_write_sector(current_root_lba, sector_buf) != 0) {
                    print("Kluda: Neizdevas atjaunot direktorijas ierakstu diskā!\n");
                    return -1;
                }

                return 0; // Veiksmīgi izdzēsts!
            }
        }
    }
    
    print("Kluda: Fails netika atrasts, lai to izdzestu!\n");
    return -1;
}


int mkdir_fat16(const char* name) {
	if (root_dir_lba == 0) {
        print("Kluda: FAT16 sistema nav inicializeta!\n");
        return -1;
    }
    uint16_t free_cluster = find_free_cluster();
    if (free_cluster == 0) {
        print("Kluda: Nav brivu klasteru diskā!\n");
        return -1;
    }

    uint32_t sectors_count;
    uint32_t dir_lba = get_dir_lba(current_dir_cluster, &sectors_count);
    uint8_t sector_buf[512];
    FAT16_DirEntry* entry;
    int found_entry = 0;
    uint32_t entry_lba = 0;

    // 1. Meklējam brīvu vietu pašreizējā direktorijā
    for (uint32_t s = 0; s < sectors_count; s++) {
        if (ata_read_sector(dir_lba + s, sector_buf) != 0) return -1;
        for (int i = 0; i < 16; i++) {
            entry = (FAT16_DirEntry*)&sector_buf[i * 32];
            if (entry->filename[0] == 0x00 || (uint8_t)entry->filename[0] == 0xE5) {
                entry_lba = dir_lba + s;
                found_entry = 1;
                break;
            }
        }
        if (found_entry) break;
    }

    if (!found_entry) {
        print("Kluda: Pašreizējā mape ir pilna!\n");
        return -1;
    }

    // 2. Aizpildām mapes ierakstu
    // Sagatavojam 8.3 nosaukumu (tīri mapes vārdu bez paplašinājuma)
    for(int j=0; j<8; j++) entry->filename[j] = ' ';
    for(int j=0; j<3; j++) entry->ext[j] = ' ';
    int len = 0;
    while(name[len] != '\0' && len < 8) {
        char c = name[len];
        if(c >= 'a' && c <= 'z') c -= 32; // Uz lielajiem burtiem
        entry->filename[len] = c;
        len++;
    }
    entry->attributes = 0x10; // 0x10 = ATTR_DIRECTORY (Svarīgākais baits!)
    entry->first_cluster_low = free_cluster;
    entry->file_size = 0; // Mapēm izmērs FAT16 vienmēr ir 0

    // Saglabājam ierakstu tēva direktorijā
    if (ata_write_sector(entry_lba, sector_buf) != 0) return -1;

    // 3. Rezervējam klasteri FAT tabulā
    if (ata_read_sector(fat_lba, sector_buf) != 0) return -1;
    uint16_t* fat = (uint16_t*)sector_buf;
    fat[free_cluster] = 0xFFFF; // Atzīmējam kā ķēdes galu
    if (ata_write_sector(fat_lba, sector_buf) != 0) return -1;

    // 4. Inicializējam jauno mapi ar "." un ".." ierakstiem
    uint8_t new_dir_buf[512];
    for(int k=0; k<512; k++) new_dir_buf[k] = 0; // Notīrām visu sektoru

    // Ieraksts 0: "." (norāde uz sevi)
    FAT16_DirEntry* dot = (FAT16_DirEntry*)&new_dir_buf[0];
    dot->filename[0] = '.'; for(int j=1; j<8; j++) dot->filename[j] = ' ';
    for(int j=0; j<3; j++) dot->ext[j] = ' ';
    dot->attributes = 0x10;
    dot->first_cluster_low = free_cluster;

    // Ieraksts 1: ".." (norāde uz tēvu)
    FAT16_DirEntry* dotdot = (FAT16_DirEntry*)&new_dir_buf[32];
    dotdot->filename[0] = '.'; dotdot->filename[1] = '.'; for(int j=2; j<8; j++) dotdot->filename[j] = ' ';
    for(int j=0; j<3; j++) dotdot->ext[j] = ' ';
    dotdot->attributes = 0x10;
    dotdot->first_cluster_low = current_dir_cluster; // Saglabājam pašreizējo kā tēvu

    // Ierakstām šo sektoru jaunās mapes pirmajā klasterī
    uint32_t dummy;
    uint32_t new_dir_lba = get_dir_lba(free_cluster, &dummy);
    if (ata_write_sector(new_dir_lba, new_dir_buf) != 0) return -1;

    return 0;
}


int change_directory(const char* name) {
    // Speciāls gadījums: "cd .." atgriež mūs atpakaļ
    // (Var apstrādāt arī šeit vai meklēt tabulā)
    
    uint32_t sectors_count;
    uint32_t dir_lba = get_dir_lba(current_dir_cluster, &sectors_count);
    uint8_t sector_buf[512];
    FAT16_DirEntry* entry;

    char clean_name[10];
    int idx = 0;
while(name[idx] != '\0' && name[idx] != ' ' && name[idx] != '\n' && name[idx] != '\r' && idx < 8) {
        char c = name[idx];
        if(c >= 'a' && c <= 'z') c -= 32;
        clean_name[idx] = c;
        idx++;
    }
    clean_name[idx] = '\0';

    // Ja raksta "cd .." manuāli
if (strcmp(clean_name, "..") == 0 && current_dir_cluster == 0) {
        print("Jus jau esat saknes mape!\n");
        return 0; 
    }

    for (uint32_t s = 0; s < sectors_count; s++) {
        if (ata_read_sector(dir_lba + s, sector_buf) != 0) return -1;
        for (int i = 0; i < 16; i++) {
            entry = (FAT16_DirEntry*)&sector_buf[i * 32];
            if (entry->filename[0] == 0x00) continue;

            // Konvertējam ieraksta nosaukumu salīdzināšanai
            char disk_name[9];
            int k = 0;
            for(int j=0; j<8; j++) if(entry->filename[j] != ' ') disk_name[k++] = entry->filename[j];
            disk_name[k] = '\0';

            // Speciāla pārbaude priekš "." un ".."
            if(entry->filename[0] == '.' && entry->filename[1] == '.') {
                disk_name[0] = '.'; disk_name[1] = '.'; disk_name[2] = '\0';
            } else if(entry->filename[0] == '.') {
                disk_name[0] = '.'; disk_name[1] = '\0';
            }

            if (strcmp(clean_name, disk_name) == 0) {
                if (entry->attributes & 0x10) { 
                    current_dir_cluster = entry->first_cluster_low;
                    return 0;
                } else {
                    print("Kluda: Tas nav mapes nosaukums, tas ir fails!\n");
                    return -1;
                }
            }
        }
    }

    // === OBLIGĀTI PIELIEC ŠO FUNKCIJAS BEIGĀS ===
    // Ja cikls beidzās un mape netika atrasta:
    print("Kluda: Mape netika atrasta!\n");
    return -1;
}
