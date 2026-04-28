#include "lcd.h"

#define FB_DEV                           "/dev/fb0"          // LCD设备节点

static int width;                                           // LCD宽度
static int height;                                          // LCD高度
static unsigned short *screen_base;                         // LCD显存基地址
static int fb_fd = -1;                                            // LCD文件描述符

int fb_dev_init(void)
{
    struct fb_var_screeninfo fb_var = {0};
    struct fb_fix_screeninfo fb_fix = {0};
    unsigned long screen_size;
    // 打开屏幕
    fb_fd = open(FB_DEV,O_RDWR);
    if (0 > fb_fd)
    {
        fprintf(stderr,"open error: %s: %s\n",FB_DEV,strerror(errno));
        return -1;
    }

    // 获取屏幕设备信息
    ioctl(fb_fd,FBIOGET_VSCREENINFO,&fb_var);
    ioctl(fb_fd,FBIOGET_FSCREENINFO,&fb_fix);
    screen_size = fb_fix.line_length * fb_var.yres;
    width = fb_var.xres;
    height = fb_var.yres ;

    // 内存映射
    screen_base = mmap(NULL,screen_size,PROT_READ | 
                       PROT_WRITE,MAP_SHARED,fb_fd,0);
    if (MAP_FAILED == (void *)screen_base)
    {
        perror("mmap error:");
        close(fb_fd);
        return -1;
    }

    // LCD屏幕刷白
    memset(screen_base,0xFF,screen_size);
    return 0;
}

int get_lcd_width(void)
{
    return width;
}

int get_lcd_height(void)
{
    return height;
}

unsigned short *get_lcd_screen_base(void)
{
    return screen_base;
}
