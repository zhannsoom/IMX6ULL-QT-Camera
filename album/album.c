#include "album.h"

#include "ui.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PHOTO_DIR "photo"

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

void album_init(album_state_t *album)
{
    if (!album)
        return;
    album->files = NULL;
    album->count = 0;
    album->index = 0;
}

void album_free(album_state_t *album)
{
    int i;

    if (!album)
        return;

    for (i = 0; i < album->count; i++)
        free(album->files[i]);
    free(album->files);
    album_init(album);
}

static int lower_char(int ch)
{
    if (ch >= 'A' && ch <= 'Z')
        return ch - 'A' + 'a';
    return ch;
}

static int has_bmp_ext(const char *name)
{
    size_t len;

    if (!name)
        return 0;
    len = strlen(name);
    if (len < 4)
        return 0;
    return name[len - 4] == '.' &&
           lower_char(name[len - 3]) == 'b' &&
           lower_char(name[len - 2]) == 'm' &&
           lower_char(name[len - 1]) == 'p';
}

static int compare_names(const void *a, const void *b)
{
    const char * const *pa = (const char * const *)a;
    const char * const *pb = (const char * const *)b;
    return strcmp(*pa, *pb);
}

int album_refresh(album_state_t *album)
{
    DIR *dir;
    struct dirent *entry;
    int old_index;

    if (!album)
        return -1;

    old_index = album->index;
    album_free(album);

    dir = opendir(PHOTO_DIR);
    if (!dir) {
        if (errno != ENOENT)
            fprintf(stderr, "album: opendir %s failed: %s\n", PHOTO_DIR,
                    strerror(errno));
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        char **new_files;
        char path[256];

        if (!has_bmp_ext(entry->d_name))
            continue;

        snprintf(path, sizeof(path), PHOTO_DIR "/%s", entry->d_name);
        new_files = (char **)realloc(album->files,
                                     sizeof(char *) * (album->count + 1));
        if (!new_files) {
            closedir(dir);
            return -1;
        }
        album->files = new_files;
        album->files[album->count] = strdup(path);
        if (!album->files[album->count]) {
            closedir(dir);
            return -1;
        }
        album->count++;
    }

    closedir(dir);

    if (album->count > 1)
        qsort(album->files, album->count, sizeof(char *), compare_names);

    if (album->count == 0)
        album->index = 0;
    else if (old_index >= album->count)
        album->index = album->count - 1;
    else if (old_index >= 0)
        album->index = old_index;
    else
        album->index = 0;

    return album->count;
}

void album_next(album_state_t *album)
{
    if (!album || album->count <= 0)
        return;
    album->index = (album->index + 1) % album->count;
}

void album_prev(album_state_t *album)
{
    if (!album || album->count <= 0)
        return;
    album->index = (album->index + album->count - 1) % album->count;
}

static uint16_t bgr_to_rgb565(unsigned char b, unsigned char g, unsigned char r)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static int load_bmp_rgb565(const char *path, uint16_t **pixels,
                           int *width, int *height)
{
    bmp_file_header_t file_header;
    bmp_info_header_t info_header;
    FILE *fp;
    uint16_t *out;
    int abs_height;
    int row_bytes;
    int padding;
    int row;
    int col;
    int top_down;

    fp = fopen(path, "rb");
    if (!fp)
        return -1;

    if (fread(&file_header, sizeof(file_header), 1, fp) != 1 ||
        fread(&info_header, sizeof(info_header), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }

    if (file_header.bfType != 0x4D42 || info_header.biBitCount != 24 ||
        info_header.biCompression != 0 || info_header.biWidth <= 0 ||
        info_header.biHeight == 0) {
        fclose(fp);
        return -1;
    }

    abs_height = info_header.biHeight < 0 ? -info_header.biHeight :
                                           info_header.biHeight;
    top_down = info_header.biHeight < 0;
    row_bytes = info_header.biWidth * 3;
    padding = (4 - (row_bytes % 4)) % 4;

    out = (uint16_t *)malloc(sizeof(uint16_t) * info_header.biWidth * abs_height);
    if (!out) {
        fclose(fp);
        return -1;
    }

    if (fseek(fp, file_header.bfOffBits, SEEK_SET) != 0) {
        free(out);
        fclose(fp);
        return -1;
    }

    for (row = 0; row < abs_height; row++) {
        int dst_row = top_down ? row : abs_height - row - 1;
        for (col = 0; col < info_header.biWidth; col++) {
            unsigned char bgr[3];
            if (fread(bgr, sizeof(bgr), 1, fp) != 1) {
                free(out);
                fclose(fp);
                return -1;
            }
            out[dst_row * info_header.biWidth + col] =
                bgr_to_rgb565(bgr[0], bgr[1], bgr[2]);
        }
        if (padding > 0 && fseek(fp, padding, SEEK_CUR) != 0) {
            free(out);
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);
    *pixels = out;
    *width = info_header.biWidth;
    *height = abs_height;
    return 0;
}

static void draw_scaled_image(uint16_t *screen, int screen_w, int screen_h,
                              const uint16_t *image, int img_w, int img_h,
                              int x, int y, int draw_w, int draw_h)
{
    int dy;
    int dx;

    if (!screen || !image || draw_w <= 0 || draw_h <= 0)
        return;

    for (dy = 0; dy < draw_h; dy++) {
        int sy = dy * img_h / draw_h;
        int py = y + dy;
        if (py < 0 || py >= screen_h)
            continue;
        for (dx = 0; dx < draw_w; dx++) {
            int sx = dx * img_w / draw_w;
            int px = x + dx;
            if (px < 0 || px >= screen_w)
                continue;
            screen[py * screen_w + px] = image[sy * img_w + sx];
        }
    }
}

int album_draw_current(album_state_t *album, uint16_t *screen,
                       int screen_width, int screen_height,
                       int top_margin, int bottom_margin)
{
    uint16_t *image = NULL;
    int img_w = 0;
    int img_h = 0;
    int area_w = screen_width;
    int area_h = screen_height - top_margin - bottom_margin;
    int draw_w;
    int draw_h;
    int x;
    int y;

    if (!screen || screen_width <= 0 || screen_height <= 0)
        return -1;

    ui_fill_screen(screen, screen_width, screen_height, UI_RGB565(7, 10, 14));

    if (!album || album->count <= 0) {
        ui_draw_text(screen, screen_width, screen_height,
                     screen_width / 2 - 84, screen_height / 2 - 12,
                     "NO PHOTO", 3, UI_RGB565(255, 255, 255));
        return 0;
    }

    if (album->index < 0)
        album->index = 0;
    if (album->index >= album->count)
        album->index = album->count - 1;

    if (load_bmp_rgb565(album->files[album->index], &image, &img_w, &img_h) < 0) {
        ui_draw_text(screen, screen_width, screen_height,
                     screen_width / 2 - 96, screen_height / 2 - 12,
                     "LOAD FAILED", 3, UI_RGB565(255, 255, 255));
        return -1;
    }

    if ((long)area_w * img_h <= (long)area_h * img_w) {
        draw_w = area_w;
        draw_h = img_h * draw_w / img_w;
    } else {
        draw_h = area_h;
        draw_w = img_w * draw_h / img_h;
    }

    x = (screen_width - draw_w) / 2;
    y = top_margin + (area_h - draw_h) / 2;
    draw_scaled_image(screen, screen_width, screen_height,
                      image, img_w, img_h, x, y, draw_w, draw_h);

    free(image);
    return 0;
}
