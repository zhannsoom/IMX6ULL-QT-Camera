# ==========================================
# V4L2 Camera 项目 Makefile
# Linux ARM 应用程序交叉编译版
# ==========================================

# 1. 交叉编译器前缀
CROSS_COMPILE ?= arm-linux-gnueabihf-

# 2. 编译器
CC := $(CROSS_COMPILE)gcc

# 3. 最终生成的可执行文件名
TARGET := v4l2_test

# 4. 头文件目录
INCDIRS := . 		\
           lcd 		\
           v4l2 	\
		   album 	\
		   input 	\
		   photo 	\
		   ui 		\
		   video    \

# 5. 源文件目录
SRCDIRS := . 		\
           lcd 		\
           v4l2		\
		   album 	\
		   input 	\
		   photo 	\
		   ui 		\
		   video    \

# 6. 编译选项
CFLAGS := -Wall -O2 -std=gnu99

# 7. 生成头文件包含参数
# 例如：-I. -Ilcd -Iv4l2
INCLUDE := $(patsubst %, -I%, $(INCDIRS))

# 8. 自动搜索所有 .c 文件
SRCS := $(foreach dir, $(SRCDIRS), $(wildcard $(dir)/*.c))
SRCS := $(filter-out ./v4l2_camera.c, $(SRCS))

# 9. 把 .c 文件转换为 obj 目录下的 .o 文件
# 例如：
# main.c              -> obj/main.o
# lcd/lcd.c           -> obj/lcd/lcd.o
# v4l2/v4l2_camera.c  -> obj/v4l2/v4l2_camera.o
OBJS := $(patsubst %.c, obj/%.o, $(SRCS))

# 10. 默认目标
.PHONY: all
all: $(TARGET)

# 11. 链接生成最终可执行文件
$(TARGET): $(OBJS)
	$(CC) -o $@ $^

# 12. 编译每一个 .c 文件为 .o 文件
obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

# 13. 清理生成文件
.PHONY: clean
clean:
	rm -rf obj $(TARGET)

# 14. 打印调试信息，可选
.PHONY: print
print:
	@echo "CC      = $(CC)"
	@echo "TARGET  = $(TARGET)"
	@echo "INCLUDE = $(INCLUDE)"
	@echo "SRCS    = $(SRCS)"
	@echo "OBJS    = $(OBJS)"
