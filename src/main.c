#include <gb/gb.h>
#include <gbdk/font.h>

#include "app.h"

void main(void) {
    font_t title_font;

    DISPLAY_OFF;

    font_init();
    title_font = font_load(font_min);
    font_set(title_font);

    BGP_REG = DMG_PALETTE(DMG_WHITE, DMG_LITE_GRAY, DMG_DARK_GRAY, DMG_BLACK);
    OBP0_REG = DMG_PALETTE(DMG_WHITE, DMG_LITE_GRAY, DMG_DARK_GRAY, DMG_BLACK);

    SHOW_BKG;
    SHOW_SPRITES;
    DISPLAY_ON;

    app_init();

    while (1) {
        app_tick();
    }
}
