#include "v4l2_camera.h"

#include "album.h"
#include "input/touch.h"
#include "lcd.h"
#include "photo.h"
#include "ui.h"
#include "video.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#define FRAMEBUFFER_COUNT 3
#define CAM_FD "/dev/video1"
#define CAMERA_FPS 30

typedef struct cameraformat {
    unsigned char description[32];
    unsigned int pixelformat;
} cam_fmt;

typedef struct cam_buf_info {
    uint16_t *start;
    unsigned long length;
} cam_buf_info;

typedef enum app_mode {
    APP_MODE_CAMERA = 0,
    APP_MODE_ALBUM
} app_mode_t;

static int width;
static int height;
static int v4l2_fd = -1;
static int frm_width;
static int frm_height;
static uint16_t *screen_base;
static cam_fmt cam_fmts[10];
static cam_buf_info buf_infos[FRAMEBUFFER_COUNT];

static int app_toolbar_height(void)
{
    return height < 360 ? 64 : 84;
}

int v4l2_dev_init(void)
{
    struct v4l2_capability cap = {0};
    const char *device = getenv("CAM_DEV");

    if (!device || device[0] == '\0')
        device = CAM_FD;

    v4l2_fd = open(device, O_RDWR);
    if (v4l2_fd < 0) {
        fprintf(stderr, "open error: %s: %s\n", device, strerror(errno));
        return -1;
    }

    if (ioctl(v4l2_fd, VIDIOC_QUERYCAP, &cap) < 0) {
        fprintf(stderr, "ioctl error: VIDIOC_QUERYCAP: %s\n", strerror(errno));
        close(v4l2_fd);
        v4l2_fd = -1;
        return -1;
    }

    if (!(V4L2_CAP_VIDEO_CAPTURE & cap.capabilities)) {
        fprintf(stderr, "Error: %s: No video capture device\n", device);
        close(v4l2_fd);
        v4l2_fd = -1;
        return -1;
    }

    return 0;
}

void v4l2_enum_formats(void)
{
    struct v4l2_fmtdesc fmtdesc = {0};

    memset(cam_fmts, 0, sizeof(cam_fmts));
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmtdesc.index = 0;

    while (fmtdesc.index < 10 &&
           ioctl(v4l2_fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
        cam_fmts[fmtdesc.index].pixelformat = fmtdesc.pixelformat;
        snprintf((char *)cam_fmts[fmtdesc.index].description,
                 sizeof(cam_fmts[fmtdesc.index].description), "%s",
                 fmtdesc.description);
        fmtdesc.index++;
    }
}

void v4l2_print_formats(void)
{
    struct v4l2_frmsizeenum frmsize = {0};
    struct v4l2_frmivalenum frmival = {0};
    int i;

    frmsize.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    frmival.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    for (i = 0; i < 10 && cam_fmts[i].pixelformat; i++) {
        printf("format<0x%x>, description<%s>\n", cam_fmts[i].pixelformat,
               cam_fmts[i].description);

        frmsize.index = 0;
        frmsize.pixel_format = cam_fmts[i].pixelformat;
        frmival.pixel_format = cam_fmts[i].pixelformat;

        while (ioctl(v4l2_fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0) {
            printf("size<%d * %d>\n", frmsize.discrete.width,
                   frmsize.discrete.height);
            frmival.index = 0;
            frmival.width = frmsize.discrete.width;
            frmival.height = frmsize.discrete.height;
            while (ioctl(v4l2_fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) == 0) {
                if (frmival.discrete.numerator != 0)
                    printf("<%dfps>\n", frmival.discrete.denominator /
                           frmival.discrete.numerator);
                frmival.index++;
            }
            frmsize.index++;
        }
        printf("\n");
    }
}

int v4l2_set_formats(void)
{
    struct v4l2_format fmt = {0};
    struct v4l2_streamparm streamparm = {0};

    width = get_lcd_width();
    height = get_lcd_height();

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;

    if (ioctl(v4l2_fd, VIDIOC_S_FMT, &fmt) < 0) {
        fprintf(stderr, "ioctl error: VIDIOC_S_FMT: %s\n", strerror(errno));
        return -1;
    }

    if (V4L2_PIX_FMT_RGB565 != fmt.fmt.pix.pixelformat) {
        fprintf(stderr, "Error: camera does not support RGB565 format\n");
        return -1;
    }

    frm_width = fmt.fmt.pix.width;
    frm_height = fmt.fmt.pix.height;
    printf("video frame size <%d * %d>\n", frm_width, frm_height);

    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(v4l2_fd, VIDIOC_G_PARM, &streamparm) == 0 &&
        (V4L2_CAP_TIMEPERFRAME & streamparm.parm.capture.capability)) {
        streamparm.parm.capture.timeperframe.numerator = 1;
        streamparm.parm.capture.timeperframe.denominator = CAMERA_FPS;
        if (ioctl(v4l2_fd, VIDIOC_S_PARM, &streamparm) < 0) {
            fprintf(stderr, "ioctl error: VIDIOC_S_PARM: %s\n", strerror(errno));
            return -1;
        }
    }

    return 0;
}

int v4l2_init_buffer(void)
{
    struct v4l2_requestbuffers reqbuf = {0};
    struct v4l2_buffer buf = {0};

    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    reqbuf.count = FRAMEBUFFER_COUNT;
    if (ioctl(v4l2_fd, VIDIOC_REQBUFS, &reqbuf) < 0) {
        fprintf(stderr, "ioctl error: VIDIOC_REQBUFS: %s\n", strerror(errno));
        return -1;
    }

    if (reqbuf.count < FRAMEBUFFER_COUNT) {
        fprintf(stderr, "Error: not enough V4L2 buffers\n");
        return -1;
    }

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    for (buf.index = 0; buf.index < FRAMEBUFFER_COUNT; buf.index++) {
        if (ioctl(v4l2_fd, VIDIOC_QUERYBUF, &buf) < 0) {
            fprintf(stderr, "ioctl error: VIDIOC_QUERYBUF: %s\n", strerror(errno));
            return -1;
        }

        buf_infos[buf.index].length = buf.length;
        buf_infos[buf.index].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                          MAP_SHARED, v4l2_fd, buf.m.offset);
        if (MAP_FAILED == buf_infos[buf.index].start) {
            perror("mmap error");
            return -1;
        }
    }

    for (buf.index = 0; buf.index < FRAMEBUFFER_COUNT; buf.index++) {
        if (ioctl(v4l2_fd, VIDIOC_QBUF, &buf) < 0) {
            fprintf(stderr, "ioctl error: VIDIOC_QBUF: %s\n", strerror(errno));
            return -1;
        }
    }

    return 0;
}

int v4l2_stream_on(void)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(v4l2_fd, VIDIOC_STREAMON, &type) < 0) {
        fprintf(stderr, "ioctl error: VIDIOC_STREAMON: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

static void copy_frame_to_lcd(const uint16_t *frame)
{
    int min_w = MIN(frm_width, width);
    int min_h = MIN(frm_height, height);
    int row;

    for (row = 0; row < min_h; row++) {
        memcpy(screen_base + row * width, frame + row * frm_width,
               (size_t)min_w * sizeof(uint16_t));
    }
}

static void handle_camera_action(ui_action_t action, const uint16_t *frame,
                                 app_mode_t *mode, album_state_t *album,
                                 int *running, int *photo_flash,
                                 int *album_dirty)
{
    char path[160];

    switch (action) {
    case UI_ACTION_PHOTO:
        if (photo_save_bmp(frame, frm_width, frm_height, frm_width,
                           path, sizeof(path)) == 0) {
            *photo_flash = 6;
        }
        break;
    case UI_ACTION_RECORD_TOGGLE:
        if (video_is_recording()) {
            video_stop();
        } else {
            video_start(frm_width, frm_height, CAMERA_FPS, path, sizeof(path));
        }
        break;
    case UI_ACTION_ALBUM:
        if (video_is_recording())
            video_stop();
        album_refresh(album);
        *mode = APP_MODE_ALBUM;
        *album_dirty = 1;
        break;
    case UI_ACTION_EXIT:
        *running = 0;
        break;
    default:
        break;
    }
}

static void handle_album_action(ui_action_t action, app_mode_t *mode,
                                album_state_t *album, int *album_dirty)
{
    switch (action) {
    case UI_ACTION_ALBUM_PREV:
        album_prev(album);
        *album_dirty = 1;
        break;
    case UI_ACTION_ALBUM_NEXT:
        album_next(album);
        *album_dirty = 1;
        break;
    case UI_ACTION_BACK:
        *mode = APP_MODE_CAMERA;
        *album_dirty = 0;
        break;
    default:
        break;
    }
}

void v4l2_read_data(void)
{
    struct v4l2_buffer buf;
    app_mode_t mode = APP_MODE_CAMERA;
    album_state_t album;
    touch_event_t touch;
    int running = 1;
    int photo_flash = 0;
    int album_dirty = 1;

    screen_base = get_lcd_screen_base();
    album_init(&album);
    photo_init();
    video_init();
    touch_init(NULL, width, height);

    while (running) {
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (ioctl(v4l2_fd, VIDIOC_DQBUF, &buf) < 0) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "ioctl error: VIDIOC_DQBUF: %s\n", strerror(errno));
            break;
        }

        if (buf.index >= FRAMEBUFFER_COUNT) {
            fprintf(stderr, "v4l2: invalid buffer index %u\n", buf.index);
            break;
        }

        if (mode == APP_MODE_CAMERA) {
            int poll_ret;
            while ((poll_ret = touch_poll(&touch)) > 0) {
                ui_action_t action = ui_camera_hit(touch.x, touch.y, width, height);
                handle_camera_action(action, buf_infos[buf.index].start, &mode,
                                     &album, &running, &photo_flash,
                                     &album_dirty);
            }
        } else {
            int poll_ret;
            while ((poll_ret = touch_poll(&touch)) > 0) {
                ui_action_t action = ui_album_hit(touch.x, touch.y, width, height);
                handle_album_action(action, &mode, &album, &album_dirty);
            }
        }

        if (!running) {
            ioctl(v4l2_fd, VIDIOC_QBUF, &buf);
            break;
        }

        if (mode == APP_MODE_CAMERA) {
            copy_frame_to_lcd(buf_infos[buf.index].start);
            if (video_is_recording() &&
                video_write_frame(buf_infos[buf.index].start, frm_width,
                                  frm_height, frm_width) < 0) {
                fprintf(stderr, "video: write frame failed, stop recording\n");
                video_stop();
            }
            ui_draw_camera_controls(screen_base, width, height,
                                    video_is_recording(), photo_flash,
                                    video_frame_count());
            if (photo_flash > 0)
                photo_flash--;
        } else if (album_dirty) {
            int top = 34;
            int bottom = app_toolbar_height();
            album_draw_current(&album, screen_base, width, height, top, bottom);
            ui_draw_album_controls(screen_base, width, height,
                                   album.index, album.count);
            album_dirty = 0;
        }

        if (ioctl(v4l2_fd, VIDIOC_QBUF, &buf) < 0) {
            fprintf(stderr, "ioctl error: VIDIOC_QBUF: %s\n", strerror(errno));
            break;
        }
    }

    if (video_is_recording())
        video_stop();
    touch_close();
    album_free(&album);
}
