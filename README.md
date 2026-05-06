# IMX6ULL-QT-Camera

这是一个基于 Linux framebuffer、V4L2 和 input event 的嵌入式 UI 相机项目，面向 I.MX6U/ARM Linux 开发板场景。项目实现了摄像头实时预览、触摸 UI、拍照、录像和相册浏览功能。

## 功能特性

- 摄像头实时预览：通过 V4L2 获取 RGB565 视频帧，并直接显示到 framebuffer。
- 触摸屏 UI：通过 `/dev/input/eventX` 获取触摸坐标，识别拍照、录像、相册和退出操作。
- 拍照功能：保存当前摄像头帧为 BMP 文件。
- 录像功能：保存 RGB565 原始帧流为 `.r565` 文件。
- 相册功能：扫描 `photo/` 目录下的 BMP 图片，支持上一张、下一张和返回。

## 运行环境

- Linux framebuffer：例如 `/dev/fb0`
- V4L2 摄像头：例如 `/dev/video1`
- Linux input 触摸屏：例如 `/dev/input/event0`
- GCC 或 ARM 交叉编译工具链

## 工程结构

```text
.
├── main.c                  程序入口
├── Makefile                编译脚本
├── lcd/                    framebuffer 初始化和显存访问
├── v4l2/                   摄像头初始化、取帧和主循环
├── input/                  触摸屏事件读取
├── ui/                     UI 绘制和按钮命中判断
├── photo/                  BMP 拍照保存
├── video/                  RGB565 原始帧录像
├── album/                  BMP 相册浏览
└── docs/                   项目文档
```

## 编译

在 Ubuntu 交叉编译：

```sh
make clean
make
```

在开发板本机编译：

```sh
make clean
make CROSS_COMPILE=
```

## 运行

```sh
CAM_DEV=/dev/video1 TOUCH_DEV=/dev/input/event0 ./v4l2_test
```

如果摄像头或触摸屏节点不同，可以修改环境变量：

```sh
CAM_DEV=/dev/video0 TOUCH_DEV=/dev/input/event1 ./v4l2_test
```

## 输出文件

程序运行时会在当前目录下自动使用：

```text
photo/    保存 BMP 照片
video/    保存 .r565 录像文件
```

## Qt 版本

新增 Qt Widgets 版本位于：

- [qt_camera/](qt_camera/)

Qt 版本使用独立预览控件和独立 UI 覆盖层，解决 framebuffer 手动画 UI 时底部功能栏被摄像头帧覆盖导致的闪屏问题，并增加广东工业大学 Logo 开机页。编译运行说明见：

- [Qt 相机说明](qt_camera/README_QT_CAMERA.md)

## 版本

当前版本：`V1`

V1 已实现：

- framebuffer 摄像头预览
- 基础触摸 UI
- 拍照保存 BMP
- 录像保存 RGB565 原始帧
- 相册浏览 BMP 图片

后续可扩展方向：

- 增加 YUYV/MJPEG 摄像头格式兼容
- 增加触摸坐标校准
- 增加标准 AVI/MP4 视频封装
- 增加视频回放和照片删除功能
