#ifndef __V4L2_CAMERA_H
#define __V4L2_CAMERA_H

#include "lcd.h"
#define MIN(a, b) ((a) < (b) ? (a) : (b))

int v4l2_dev_init(void);
void v4l2_enum_formats(void);
void v4l2_print_formats(void);
int v4l2_set_formats(void);
int v4l2_init_buffer(void);
int v4l2_stream_on(void);
void v4l2_read_data(void);



#endif 