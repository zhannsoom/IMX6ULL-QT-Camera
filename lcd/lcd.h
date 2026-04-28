#ifndef __LCD_H
#define __LCD_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h> 
#include <linux/fb.h>


int fb_dev_init(void);
int get_lcd_width(void);
int get_lcd_height(void);
unsigned short *get_lcd_screen_base(void);




#endif 