#include "main.h"

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (fb_dev_init() != 0)
        return -1;

    if (v4l2_dev_init() != 0)
        return -1;

    v4l2_enum_formats();
    v4l2_print_formats();

    if (v4l2_set_formats() != 0)
        return -1;

    if (v4l2_init_buffer() != 0)
        return -1;

    if (v4l2_stream_on() != 0)
        return -1;

    printf("Camera UI ready. CAM_DEV and TOUCH_DEV can override devices.\n");
    v4l2_read_data();

    return 0;
}
