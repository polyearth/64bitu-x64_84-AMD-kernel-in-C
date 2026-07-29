#ifndef ATA_H
#define ATA_H

#include <stdint.h>

// Sektora izmērs baitos (ATA standartā vienmēr 512 baiti)
#define SECTOR_SIZE 512

// Funkcija, lai nolasītu vienu sektoru no diska (LBA28 adrešu režīms)
// lba - sektora numurs, buffer - kur saglabāt 512 nolasītos baitus
int ata_read_sector(uint32_t lba, uint8_t *buffer);

// Funkcija, lai ierakstītu vienu sektoru diskā
// lba - sektora numurs, buffer - 512 baiti, ko ierakstīt
int ata_write_sector(uint32_t lba, const uint8_t *buffer);

#endif
