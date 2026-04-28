#ifndef __TOUCH_H
#define __TOUCH_H

typedef struct touch_event {
    int x;
    int y;
} touch_event_t;

int touch_init(const char *device, int screen_width, int screen_height);
int touch_poll(touch_event_t *event);
void touch_close(void);

#endif
