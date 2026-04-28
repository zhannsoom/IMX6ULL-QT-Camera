#include "ui.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct ui_rect {
    int x;
    int y;
    int w;
    int h;
} ui_rect_t;

static const uint16_t COLOR_BLACK = UI_RGB565(0, 0, 0);
static const uint16_t COLOR_WHITE = UI_RGB565(255, 255, 255);
static const uint16_t COLOR_PANEL = UI_RGB565(19, 24, 32);
static const uint16_t COLOR_BUTTON = UI_RGB565(43, 50, 60);
static const uint16_t COLOR_BUTTON_ALT = UI_RGB565(26, 82, 88);
static const uint16_t COLOR_ACCENT = UI_RGB565(248, 190, 72);
static const uint16_t COLOR_RECORD = UI_RGB565(230, 47, 61);
static const uint16_t COLOR_MUTED = UI_RGB565(158, 168, 179);

static int toolbar_height(int height)
{
    return height < 360 ? 64 : 84;
}

static ui_rect_t camera_album_rect(int width, int height)
{
    int tb = toolbar_height(height);
    int bw = width < 520 ? 104 : 144;
    return (ui_rect_t){12, height - tb + 10, bw, tb - 20};
}

static ui_rect_t camera_photo_rect(int width, int height)
{
    int tb = toolbar_height(height);
    int bw = width < 520 ? 112 : 136;
    return (ui_rect_t){(width - bw) / 2, height - tb + 8, bw, tb - 16};
}

static ui_rect_t camera_record_rect(int width, int height)
{
    int tb = toolbar_height(height);
    int bw = width < 520 ? 104 : 144;
    return (ui_rect_t){width - bw - 12, height - tb + 10, bw, tb - 20};
}

static ui_rect_t camera_exit_rect(int width)
{
    return (ui_rect_t){width - 54, 8, 46, 46};
}

static ui_rect_t album_prev_rect(int width, int height)
{
    int tb = toolbar_height(height);
    int bw = width < 520 ? 100 : 132;
    return (ui_rect_t){12, height - tb + 10, bw, tb - 20};
}

static ui_rect_t album_back_rect(int width, int height)
{
    int tb = toolbar_height(height);
    int bw = width < 520 ? 110 : 132;
    return (ui_rect_t){(width - bw) / 2, height - tb + 10, bw, tb - 20};
}

static ui_rect_t album_next_rect(int width, int height)
{
    int tb = toolbar_height(height);
    int bw = width < 520 ? 100 : 132;
    return (ui_rect_t){width - bw - 12, height - tb + 10, bw, tb - 20};
}

static int point_in_rect(int x, int y, ui_rect_t rect)
{
    return x >= rect.x && x < rect.x + rect.w &&
           y >= rect.y && y < rect.y + rect.h;
}

void ui_fill_rect(uint16_t *screen, int width, int height,
                  int x, int y, int rect_w, int rect_h, uint16_t color)
{
    int row;
    int col;

    if (!screen || width <= 0 || height <= 0 || rect_w <= 0 || rect_h <= 0)
        return;

    if (x < 0) {
        rect_w += x;
        x = 0;
    }
    if (y < 0) {
        rect_h += y;
        y = 0;
    }
    if (x + rect_w > width)
        rect_w = width - x;
    if (y + rect_h > height)
        rect_h = height - y;
    if (rect_w <= 0 || rect_h <= 0)
        return;

    for (row = 0; row < rect_h; row++) {
        uint16_t *dst = screen + (y + row) * width + x;
        for (col = 0; col < rect_w; col++)
            dst[col] = color;
    }
}

void ui_fill_screen(uint16_t *screen, int width, int height, uint16_t color)
{
    ui_fill_rect(screen, width, height, 0, 0, width, height, color);
}

static void draw_rect_outline(uint16_t *screen, int width, int height,
                              ui_rect_t rect, uint16_t color)
{
    ui_fill_rect(screen, width, height, rect.x, rect.y, rect.w, 2, color);
    ui_fill_rect(screen, width, height, rect.x, rect.y + rect.h - 2,
                 rect.w, 2, color);
    ui_fill_rect(screen, width, height, rect.x, rect.y, 2, rect.h, color);
    ui_fill_rect(screen, width, height, rect.x + rect.w - 2, rect.y,
                 2, rect.h, color);
}

static void draw_filled_circle(uint16_t *screen, int width, int height,
                               int cx, int cy, int radius, uint16_t color)
{
    int x;
    int y;
    int r2 = radius * radius;

    for (y = -radius; y <= radius; y++) {
        int py = cy + y;
        if (py < 0 || py >= height)
            continue;
        for (x = -radius; x <= radius; x++) {
            int px = cx + x;
            if (px < 0 || px >= width)
                continue;
            if (x * x + y * y <= r2)
                screen[py * width + px] = color;
        }
    }
}

static void draw_circle_outline(uint16_t *screen, int width, int height,
                                int cx, int cy, int radius, int thickness,
                                uint16_t color)
{
    int x;
    int y;
    int r2 = radius * radius;
    int inner = radius - thickness;
    int inner2 = inner > 0 ? inner * inner : 0;

    for (y = -radius; y <= radius; y++) {
        int py = cy + y;
        if (py < 0 || py >= height)
            continue;
        for (x = -radius; x <= radius; x++) {
            int px = cx + x;
            int d2 = x * x + y * y;
            if (px < 0 || px >= width)
                continue;
            if (d2 <= r2 && d2 >= inner2)
                screen[py * width + px] = color;
        }
    }
}

static const uint8_t *glyph_rows(char c)
{
    static const uint8_t space[7] = {0, 0, 0, 0, 0, 0, 0};
    static const uint8_t slash[7] = {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10};
    static const uint8_t colon[7] = {0, 0x04, 0x04, 0, 0x04, 0x04, 0};
    static const uint8_t dash[7] = {0, 0, 0, 0x1F, 0, 0, 0};
    static const uint8_t lt[7] = {0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02};
    static const uint8_t gt[7] = {0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08};
    static const uint8_t digits[10][7] = {
        {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},
        {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
        {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},
        {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E},
        {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},
        {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
        {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E},
        {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
        {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
        {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}
    };
    static const uint8_t letters[26][7] = {
        {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
        {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},
        {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E},
        {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E},
        {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},
        {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},
        {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F},
        {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
        {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E},
        {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C},
        {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},
        {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},
        {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11},
        {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},
        {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
        {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},
        {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D},
        {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},
        {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E},
        {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
        {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
        {0x11, 0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04},
        {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11},
        {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},
        {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04},
        {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}
    };

    if (c >= 'a' && c <= 'z')
        c = (char)(c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z')
        return letters[c - 'A'];
    if (c >= '0' && c <= '9')
        return digits[c - '0'];
    if (c == '/')
        return slash;
    if (c == ':')
        return colon;
    if (c == '-')
        return dash;
    if (c == '<')
        return lt;
    if (c == '>')
        return gt;
    return space;
}

void ui_draw_text(uint16_t *screen, int width, int height,
                  int x, int y, const char *text, int scale, uint16_t color)
{
    int i;
    int row;
    int col;

    if (!screen || !text || scale <= 0)
        return;

    for (i = 0; text[i] != '\0'; i++) {
        const uint8_t *glyph = glyph_rows(text[i]);
        int base_x = x + i * 6 * scale;
        for (row = 0; row < 7; row++) {
            for (col = 0; col < 5; col++) {
                if (glyph[row] & (1 << (4 - col))) {
                    ui_fill_rect(screen, width, height,
                                 base_x + col * scale, y + row * scale,
                                 scale, scale, color);
                }
            }
        }
    }
}

static int text_width(const char *text, int scale)
{
    if (!text)
        return 0;
    return (int)strlen(text) * 6 * scale - scale;
}

static void draw_center_text(uint16_t *screen, int width, int height,
                             ui_rect_t rect, const char *text, int scale,
                             uint16_t color)
{
    int tw = text_width(text, scale);
    int th = 7 * scale;
    int x = rect.x + (rect.w - tw) / 2;
    int y = rect.y + (rect.h - th) / 2;
    ui_draw_text(screen, width, height, x, y, text, scale, color);
}

static void draw_button(uint16_t *screen, int width, int height,
                        ui_rect_t rect, uint16_t fill, uint16_t border,
                        const char *label)
{
    int scale = rect.h < 52 ? 1 : 2;

    ui_fill_rect(screen, width, height, rect.x, rect.y, rect.w, rect.h, fill);
    draw_rect_outline(screen, width, height, rect, border);
    draw_center_text(screen, width, height, rect, label, scale, COLOR_WHITE);
}

static void draw_album_icon(uint16_t *screen, int width, int height,
                            ui_rect_t rect)
{
    int icon_w = rect.w / 3;
    int icon_h = rect.h / 3;
    int x = rect.x + 14;
    int y = rect.y + (rect.h - icon_h) / 2;

    ui_fill_rect(screen, width, height, x + 8, y - 6, icon_w, icon_h,
                 UI_RGB565(76, 86, 101));
    draw_rect_outline(screen, width, height,
                      (ui_rect_t){x + 8, y - 6, icon_w, icon_h}, COLOR_MUTED);
    ui_fill_rect(screen, width, height, x, y, icon_w, icon_h,
                 UI_RGB565(55, 65, 81));
    draw_rect_outline(screen, width, height,
                      (ui_rect_t){x, y, icon_w, icon_h}, COLOR_WHITE);
}

static void draw_record_icon(uint16_t *screen, int width, int height,
                             ui_rect_t rect, int recording)
{
    int size = rect.h / 3;
    int cx = rect.x + 22;
    int cy = rect.y + rect.h / 2;

    if (recording) {
        ui_fill_rect(screen, width, height, cx - size / 2, cy - size / 2,
                     size, size, COLOR_RECORD);
    } else {
        draw_filled_circle(screen, width, height, cx, cy, size / 2,
                           COLOR_RECORD);
    }
}

void ui_draw_camera_controls(uint16_t *screen, int width, int height,
                             int recording, int photo_flash_frames,
                             unsigned int record_frames)
{
    int tb = toolbar_height(height);
    ui_rect_t album = camera_album_rect(width, height);
    ui_rect_t photo = camera_photo_rect(width, height);
    ui_rect_t record = camera_record_rect(width, height);
    ui_rect_t exit_btn = camera_exit_rect(width);
    char rec_text[24];
    int radius = photo.h < photo.w ? photo.h / 2 - 8 : photo.w / 2 - 8;

    ui_fill_rect(screen, width, height, 0, height - tb, width, tb, COLOR_PANEL);
    draw_button(screen, width, height, album, COLOR_BUTTON, COLOR_MUTED, "ALBUM");
    draw_album_icon(screen, width, height, album);

    ui_fill_rect(screen, width, height, photo.x, photo.y, photo.w, photo.h,
                 UI_RGB565(13, 17, 23));
    draw_rect_outline(screen, width, height, photo,
                      photo_flash_frames > 0 ? COLOR_ACCENT : COLOR_WHITE);
    draw_circle_outline(screen, width, height,
                        photo.x + photo.w / 2, photo.y + photo.h / 2,
                        radius, 5, COLOR_WHITE);
    draw_filled_circle(screen, width, height,
                       photo.x + photo.w / 2, photo.y + photo.h / 2,
                       radius - 11, photo_flash_frames > 0 ? COLOR_ACCENT : COLOR_WHITE);

    snprintf(rec_text, sizeof(rec_text), recording ? "STOP %u" : "REC",
             record_frames);
    draw_button(screen, width, height, record,
                recording ? UI_RGB565(74, 22, 32) : COLOR_BUTTON_ALT,
                recording ? COLOR_RECORD : COLOR_MUTED, rec_text);
    draw_record_icon(screen, width, height, record, recording);

    ui_fill_rect(screen, width, height, exit_btn.x, exit_btn.y,
                 exit_btn.w, exit_btn.h, UI_RGB565(25, 30, 38));
    draw_rect_outline(screen, width, height, exit_btn, COLOR_MUTED);
    ui_draw_text(screen, width, height, exit_btn.x + 15, exit_btn.y + 12,
                 "X", 3, COLOR_WHITE);
}

void ui_draw_album_controls(uint16_t *screen, int width, int height,
                            int index, int total)
{
    int tb = toolbar_height(height);
    ui_rect_t prev = album_prev_rect(width, height);
    ui_rect_t back = album_back_rect(width, height);
    ui_rect_t next = album_next_rect(width, height);
    char title[32];

    ui_fill_rect(screen, width, height, 0, 0, width, 34, COLOR_PANEL);
    if (total > 0)
        snprintf(title, sizeof(title), "ALBUM %d/%d", index + 1, total);
    else
        snprintf(title, sizeof(title), "NO PHOTO");
    ui_draw_text(screen, width, height, 14, 10, title, 2, COLOR_WHITE);

    ui_fill_rect(screen, width, height, 0, height - tb, width, tb, COLOR_PANEL);
    draw_button(screen, width, height, prev, COLOR_BUTTON, COLOR_MUTED, "< PREV");
    draw_button(screen, width, height, back, COLOR_BUTTON_ALT, COLOR_MUTED, "BACK");
    draw_button(screen, width, height, next, COLOR_BUTTON, COLOR_MUTED, "NEXT >");
}

ui_action_t ui_camera_hit(int x, int y, int width, int height)
{
    if (point_in_rect(x, y, camera_exit_rect(width)))
        return UI_ACTION_EXIT;
    if (point_in_rect(x, y, camera_album_rect(width, height)))
        return UI_ACTION_ALBUM;
    if (point_in_rect(x, y, camera_photo_rect(width, height)))
        return UI_ACTION_PHOTO;
    if (point_in_rect(x, y, camera_record_rect(width, height)))
        return UI_ACTION_RECORD_TOGGLE;
    return UI_ACTION_NONE;
}

ui_action_t ui_album_hit(int x, int y, int width, int height)
{
    if (point_in_rect(x, y, album_prev_rect(width, height)))
        return UI_ACTION_ALBUM_PREV;
    if (point_in_rect(x, y, album_back_rect(width, height)))
        return UI_ACTION_BACK;
    if (point_in_rect(x, y, album_next_rect(width, height)))
        return UI_ACTION_ALBUM_NEXT;
    return UI_ACTION_NONE;
}
