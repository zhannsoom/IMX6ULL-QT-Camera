#ifndef __PHOTO_H
#define __PHOTO_H

#include <stddef.h>
#include <stdint.h>

int photo_init(void);
int photo_save_bmp(const uint16_t *frame, int width, int height, int stride,
                   char *out_path, size_t out_path_size);

#endif
