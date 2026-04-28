#include "touch.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int touch_fd = -1;
static int screen_w;
static int screen_h;
static int raw_x;
static int raw_y;
static int last_x;
static int last_y;
static int down;
static int was_down;
static int have_abs_info;
static struct input_absinfo abs_x_info;
static struct input_absinfo abs_y_info;

static int open_touch_device(const char *device)
{
    if (!device || device[0] == '\0')
        return -1;
    return open(device, O_RDONLY | O_NONBLOCK);
}

static int query_abs_info(int fd)
{
    memset(&abs_x_info, 0, sizeof(abs_x_info));
    memset(&abs_y_info, 0, sizeof(abs_y_info));

    if (ioctl(fd, EVIOCGABS(ABS_X), &abs_x_info) == 0 &&
        ioctl(fd, EVIOCGABS(ABS_Y), &abs_y_info) == 0)
        return 1;

    if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &abs_x_info) == 0 &&
        ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &abs_y_info) == 0)
        return 1;

    return 0;
}

int touch_init(const char *device, int screen_width, int screen_height)
{
    char path[64];
    const char *env_dev;
    int i;

    screen_w = screen_width;
    screen_h = screen_height;
    raw_x = raw_y = last_x = last_y = 0;
    down = was_down = 0;

    if (touch_fd >= 0)
        touch_close();

    env_dev = getenv("TOUCH_DEV");
    touch_fd = open_touch_device(device);
    if (touch_fd < 0)
        touch_fd = open_touch_device(env_dev);

    for (i = 0; touch_fd < 0 && i < 16; i++) {
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        touch_fd = open_touch_device(path);
        if (touch_fd >= 0 && !query_abs_info(touch_fd)) {
            close(touch_fd);
            touch_fd = -1;
        }
    }

    if (touch_fd < 0) {
        fprintf(stderr, "touch: no input device found, ui touch disabled\n");
        return -1;
    }

    have_abs_info = query_abs_info(touch_fd);
    if (!have_abs_info)
        fprintf(stderr, "touch: abs range not available, using raw coordinates\n");

    return 0;
}

static int scale_axis(int value, const struct input_absinfo *info, int size)
{
    long range;
    long scaled;

    if (!have_abs_info || size <= 0)
        return value;

    range = (long)info->maximum - (long)info->minimum;
    if (range <= 0)
        return value;

    scaled = ((long)value - (long)info->minimum) * (long)(size - 1) / range;
    if (scaled < 0)
        scaled = 0;
    if (scaled >= size)
        scaled = size - 1;
    return (int)scaled;
}

static void update_last_point(void)
{
    last_x = scale_axis(raw_x, &abs_x_info, screen_w);
    last_y = scale_axis(raw_y, &abs_y_info, screen_h);
}

int touch_poll(touch_event_t *event)
{
    struct input_event ev;
    int emitted = 0;

    if (!event || touch_fd < 0)
        return 0;

    while (read(touch_fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
        if (ev.type == EV_ABS) {
            if (ev.code == ABS_X || ev.code == ABS_MT_POSITION_X) {
                raw_x = ev.value;
                update_last_point();
            } else if (ev.code == ABS_Y || ev.code == ABS_MT_POSITION_Y) {
                raw_y = ev.value;
                update_last_point();
            } else if (ev.code == ABS_PRESSURE) {
                down = ev.value > 0;
            } else if (ev.code == ABS_MT_TRACKING_ID) {
                down = ev.value >= 0;
            }
        } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
            down = ev.value > 0;
        } else if (ev.type == EV_SYN) {
            if (was_down && !down) {
                event->x = last_x;
                event->y = last_y;
                emitted = 1;
            }
            was_down = down;
            if (emitted)
                return 1;
        }
    }

    if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
        return -1;

    return 0;
}

void touch_close(void)
{
    if (touch_fd >= 0) {
        close(touch_fd);
        touch_fd = -1;
    }
}
