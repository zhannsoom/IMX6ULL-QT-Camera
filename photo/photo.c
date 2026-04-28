#include "photo.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define PHOTO_DIR "photo"

static int make_dir(const char *path)
{
#ifdef _WIN32
    return mkdir(path);
#else
    return mkdir(path, 0755);
#endif
}

#pragma pack(push, 1)
typedef struct bmp_file_header {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} bmp_file_header_t;

typedef struct bmp_info_header {
    uint32_t biSize;
    int32_t biWidth;
    int32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t biXPelsPerMeter;
    int32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} bmp_info_header_t;
#pragma pack(pop)

int photo_init(void)
{
    if (make_dir(PHOTO_DIR) < 0 && errno != EEXIST) {
        fprintf(stderr, "photo: mkdir %s failed: %s\n", PHOTO_DIR, strerror(errno));
        return -1;
    }
    return 0;
}

static void make_photo_path(char *path, size_t path_size)
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
                     PHOTO_DIR "/IMG_%04d%02d%02d_%02d%02d%02d.bmp",
                     tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
                     tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
        } else {
            snprintf(path, path_size,
                     PHOTO_DIR "/IMG_%04d%02d%02d_%02d%02d%02d_%02d.bmp",
                     tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
                     tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec, suffix);
        }

        if (access(path, F_OK) != 0)
            return;
    }
}

static void rgb565_to_bgr(uint16_t pixel, unsigned char *bgr)
{
    unsigned int r = (pixel >> 11) & 0x1F;
    unsigned int g = (pixel >> 5) & 0x3F;
    unsigned int b = pixel & 0x1F;

    bgr[0] = (unsigned char)((b << 3) | (b >> 2));
    bgr[1] = (unsigned char)((g << 2) | (g >> 4));
    bgr[2] = (unsigned char)((r << 3) | (r >> 2));
}

int photo_save_bmp(const uint16_t *frame, int width, int height, int stride,
                   char *out_path, size_t out_path_size)
{
    bmp_file_header_t file_header;
    bmp_info_header_t info_header;
    unsigned char pad[3] = {0, 0, 0};
    unsigned char bgr[3];
    char path[128];
    FILE *fp;
    int row;
    int col;
    int row_bytes;
    int padding;
    uint32_t image_size;

    if (!frame || width <= 0 || height <= 0 || stride < width)
        return -1;

    if (photo_init() < 0)
        return -1;

    make_photo_path(path, sizeof(path));
    fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "photo: open %s failed: %s\n", path, strerror(errno));
        return -1;
    }

    row_bytes = width * 3;
    padding = (4 - (row_bytes % 4)) % 4;
    image_size = (uint32_t)((row_bytes + padding) * height);

    memset(&file_header, 0, sizeof(file_header));
    memset(&info_header, 0, sizeof(info_header));
    file_header.bfType = 0x4D42;
    file_header.bfOffBits = sizeof(file_header) + sizeof(info_header);
    file_header.bfSize = file_header.bfOffBits + image_size;

    info_header.biSize = sizeof(info_header);
    info_header.biWidth = width;
    info_header.biHeight = height;
    info_header.biPlanes = 1;
    info_header.biBitCount = 24;
    info_header.biCompression = 0;
    info_header.biSizeImage = image_size;
    info_header.biXPelsPerMeter = 2835;
    info_header.biYPelsPerMeter = 2835;

    if (fwrite(&file_header, sizeof(file_header), 1, fp) != 1 ||
        fwrite(&info_header, sizeof(info_header), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }

    for (row = height - 1; row >= 0; row--) {
        const uint16_t *src = frame + row * stride;
        for (col = 0; col < width; col++) {
            rgb565_to_bgr(src[col], bgr);
            if (fwrite(bgr, sizeof(bgr), 1, fp) != 1) {
                fclose(fp);
                return -1;
            }
        }
        if (padding > 0 && fwrite(pad, 1, padding, fp) != (size_t)padding) {
            fclose(fp);
            return -1;
        }
    }

    if (fclose(fp) != 0)
        return -1;

    if (out_path && out_path_size > 0) {
        snprintf(out_path, out_path_size, "%s", path);
    }

    printf("photo: saved %s\n", path);
    return 0;
}
