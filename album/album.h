#ifndef __ALBUM_H
#define __ALBUM_H

#include <stdint.h>

typedef struct album_state {
    char **files;
    int count;
    int index;
} album_state_t;

void album_init(album_state_t *album);
void album_free(album_state_t *album);
int album_refresh(album_state_t *album);
void album_next(album_state_t *album);
void album_prev(album_state_t *album);
int album_draw_current(album_state_t *album, uint16_t *screen,
                       int screen_width, int screen_height,
                       int top_margin, int bottom_margin);

#endif
