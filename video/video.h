#ifndef __VIDEO_H
#define __VIDEO_H

#include <stddef.h>
#include <stdint.h>

int video_init(void);
int video_start(int width, int height, int fps,
                char *out_path, size_t out_path_size);
int video_write_frame(const uint16_t *frame, int width, int height, int stride);
int video_stop(void);
int video_is_recording(void);
unsigned int video_frame_count(void);

#endif
