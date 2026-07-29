#ifndef DESKTOP_H
#define DESKTOP_H

#include <stdint.h>

// Funkcijas rādītājs (Pointer) darbībai, ko ikona veiks
typedef void (*icon_action_t)(void);

typedef struct {
    int x, y;             // Pozīcija ekrānā
    int w, h;             // Izmērs (teksta zīmēs)
    uint8_t color;        // Ikonas krāsa
    char label[12];       // Nosaukums zem ikonas
    icon_action_t action; // Funkcija, ko izsaukt uz klikšķa
} OS_Icon;

// Galvenās darbvirsmas funkcijas
void desktop_init(void);
void desktop_draw(void);
void check_desktop_clicks(void);

#endif
