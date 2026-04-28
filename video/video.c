#include "video.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define VIDEO_DIR "video"
#define VIDEO_MAGIC "R565VID1"
#define VIDEO_HEADER_SIZE 32

static int make_dir(const char *path)
{
#ifdef _WIN32
    return mkdir(path);
#else
    return mkdir(path, 0755);
#endif
}

static FILE *video_fp;
static int video_width;
static int video_height;
static int video_fps;
static unsigned int frame_count;
static char current_path[128];

static void put_u32_le(unsigned char *dst, uint32_t value)
{
    dst[0] = (unsigned char)(value & 0xFF);
    dst[1] = (unsigned char)((value >> 8) & 0xFF);
    dst[2] = (unsigned char)((value >> 16) & 0xFF);
    dst[3] = (unsigned char)((value >> 24) & 0xFF);
}

int video_init(void)
{
    if (make_dir(VIDEO_DIR) < 0 && errno != EEXIST) {
        fprintf(stderr, "video: mkdir %s failed: %s\n", VIDEO_DIR, strerror(errno));
        return -1;
    }
    return 0;
}

static void make_video_path(char *path, size_t path_size)
{
    time_t now = time(NULL);
    struct tm tm_now;
    struct tm *tm_ptr;
    int suffix;

    tm_ptr = localtime(&now);
    if (tm_ptr)
        tm_now = *tm_ptr;
    else
        memset(&tm_now, 0, sizeof(tm_now));
    for (suffix = 0; suffix < 100; suffix++) {
        if (suffix == 0) {
            snprintf(path, path_size,
                     VIDEO_DIR "/VID_%04d%02d%02d_%02d%02d%02d.r565",
                     tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
                     tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
        } else {
            snprintf(path, path_size,
                     VIDEO_DIR "/VID_%04d%02d%02d_%02d%02d%02d_%02d.r565",
                     tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
                     tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec, suffix);
        }

        if (access(path, F_OK) != 0)
            return;
    }
}

static int write_header(void)
{
    unsigned char header[VIDEO_HEADER_SIZE];

    if (!video_fp)
        return -1;

    memset(header, 0, sizeof(header));
    memcpy(header, VIDEO_MAGIC, 8);
    put_u32_le(header + 8, (uint32_t)video_width);
    put_u32_le(header + 12, (uint32_t)video_height);
    put_u32_le(header + 16, (uint32_t)video_fps);
    put_u32_le(header + 20, frame_count);
    put_u32_le(header + 24, 16);
    put_u32_le(header + 28, 0);

    if (fseek(video_fp, 0, SEEK_SET) != 0)
        return -1;
    if (fwrite(header, sizeof(header), 1, video_fp) != 1)
        return -1;
    if (fseek(video_fp, 0, SEEK_END) != 0)
        return -1;
    return 0;
}

int video_start(int width, int height, int fps,
                char *out_path, size_t out_path_size)
{
    if (width <= 0 || height <= 0)
        return -1;

    if (video_init() < 0)
        return -1;

    if (video_fp)
        video_stop();

    make_video_path(current_path, sizeof(current_path));
    video_fp = fopen(current_path, "wb+");
    if (!video_fp) {
        fprintf(stderr, "video: open %s failed: %s\n", current_path, strerror(errno));
        return -1;
    }

    video_width = width;
    video_height = height;
    video_fps = fps > 0 ? fps : 30;
    frame_count = 0;

    if (write_header() < 0) {
        fclose(video_fp);
        video_fp = NULL;
        return -1;
    }

    if (out_path && out_path_size > 0)
        snprintf(out_path, out_path_size, "%s", current_path);

    printf("video: recording %s\n", current_path);
    return 0;
}

int video_write_frame(const uint16_t *frame, int width, int height, int stride)
{
    int row;

    if (!video_fp || !frame || width != video_width || height != video_height ||
        stride < width)
        return -1;

    if (stride == width) {
        if (fwrite(frame, sizeof(uint16_t), (size_t)width * height, video_fp) !=
            (size_t)width * height)
            return -1;
    } else {
        for (row = 0; row < height; row++) {
            if (fwrite(frame + row * stride, sizeof(uint16_t), width, video_fp) !=
                (size_t)width)
                return -1;
        }
    }

    frame_count++;
    return 0;
}

int video_stop(void)
{
    if (!video_fp)
        return 0;

    if (write_header() < 0)
        fprintf(stderr, "video: failed to finalize header for %s\n", current_path);

    if (fclose(video_fp) != 0) {
        video_fp = NULL;
        return -1;
    }

    printf("video: saved %s, frames=%u\n", current_path, frame_count);
    video_fp = NULL;
    return 0;
}

int video_is_recording(void)
{
    return video_fp != NULL;
}

unsigned int video_frame_count(void)
{
    return frame_count;
}
