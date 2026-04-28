#ifndef __UI_H
#define __UI_H

#include <stdint.h>

#define UI_RGB565(r, g, b) \
    (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

typedef enum ui_action {
    UI_ACTION_NONE = 0,
    UI_ACTION_PHOTO,
    UI_ACTION_RECORD_TOGGLE,
    UI_ACTION_ALBUM,
    UI_ACTION_ALBUM_PREV,
    UI_ACTION_ALBUM_NEXT,
    UI_ACTION_BACK,
    UI_ACTION_EXIT
} ui_action_t;

void ui_fill_screen(uint16_t *screen, int width, int height, uint16_t color);
void ui_fill_rect(uint16_t *screen, int width, int height,
                  int x, int y, int rect_w, int rect_h, uint16_t color);
void ui_draw_text(uint16_t *screen, int width, int height,
                  int x, int y, const char *text, int scale, uint16_t color);

void ui_draw_camera_controls(uint16_t *screen, int width, int height,
                             int recording, int photo_flash_frames,
                             unsigned int record_frames);
void ui_draw_album_controls(uint16_t *screen, int width, int height,
                            int index, int total);

ui_action_t ui_camera_hit(int x, int y, int width, int height);
ui_action_t ui_album_hit(int x, int y, int width, int height);

#endif
