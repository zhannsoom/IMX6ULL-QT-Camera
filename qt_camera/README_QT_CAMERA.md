# GDUT Qt Camera

这是 UI 相机的 Qt 版本。它保留 V4L2 摄像头采集能力，但把界面层从“手动写 framebuffer 像素”改成 Qt Widgets。

## 为什么 Qt 版本可以解决功能栏闪屏

原 framebuffer 版本的显示逻辑是：

```text
摄像头帧 memcpy 到 LCD 显存
        ↓
在同一块显存上画功能栏
        ↓
下一帧摄像头又 memcpy 到 LCD 显存
```

这样如果绘制时序不稳定，底部功能栏会被视频帧覆盖，看起来就像“功能栏和视频快速闪烁切换”。

Qt 版本改成：

```text
QLabel 显示摄像头预览
        ↓
Qt Widget 功能栏作为独立控件覆盖在预览上
```

视频帧只更新预览控件，不会直接覆盖功能栏控件，所以功能栏不会再被摄像头帧擦掉。

## 流畅性优化

当前版本针对开发板卡顿做了三处优化：

1. 采集线程最多只保留一帧待显示。如果 UI 线程还没处理完上一帧，新采集到的帧会被丢弃，避免 Qt 事件队列越积越多导致画面延迟。
2. 实时预览使用快速缩放 `Qt::FastTransformation`，拍照缩略图和相册仍使用高质量缩放。
3. 录像 MJPEG 编码移动到后台线程，并限制录像队列最多积压一帧，避免 JPEG 压缩阻塞 UI 刷新。

详细问题分析和解决过程见：

```text
../docs/qt_camera_optimization_report.md
```

## 功能

- 启动后先显示广东工业大学 Logo 开机页。
- 进入相机主界面后显示实时预览。
- UI 风格参考 iPhone 相机：顶部状态栏、底部黑色功能栏、模式切换、圆形快门按钮。
- PHOTO 模式：点击快门拍照，保存到 `photo/`。
- VIDEO 模式：点击快门开始/停止录像，保存到 `video/`。
- ALBUM：浏览已拍摄照片。
- 支持 `CAM_DEV` 或命令行参数指定摄像头节点。

## 编译

进入 Qt 工程目录：

```sh
cd qt_camera
```

生成 Makefile：

```sh
qmake qt_camera.pro
```

编译：

```sh
make
```

生成程序：

```text
gdut_qt_camera
```

如果是在开发板上本机编译，需要开发板文件系统已经安装 Qt Widgets 开发环境和 qmake。

## 运行

默认运行：

```sh
./gdut_qt_camera
```

指定摄像头：

```sh
./gdut_qt_camera --device /dev/video1
```

或者：

```sh
CAM_DEV=/dev/video1 ./gdut_qt_camera
```

窗口模式调试：

```sh
./gdut_qt_camera --windowed --device /dev/video1
```

指定分辨率：

```sh
./gdut_qt_camera --device /dev/video1 --width 800 --height 480 --fps 30
```

## 输出文件

照片：

```text
photo/IMG_yyyyMMdd_hhmmss.jpg
```

如果系统 Qt 没有 JPEG 插件，会自动退回保存 PNG：

```text
photo/IMG_yyyyMMdd_hhmmss.png
```

录像：

```text
video/VID_yyyyMMdd_hhmmss.avi
```

录像使用 Qt 直接写 MJPEG AVI，不依赖 FFmpeg。注意：如果开发板 Qt 没有 JPEG imageformats 插件，录像会提示失败。此时需要安装或拷贝 Qt 的 JPEG 图片插件。

## 文件结构

```text
qt_camera/
├── qt_camera.pro          qmake 工程文件
├── main.cpp               程序入口和命令行参数
├── camerawindow.*         Qt 相机主界面、开机 Logo、相册、拍照录像逻辑
├── v4l2capture.*          V4L2 采集线程
├── aviwriter.*            MJPEG AVI 写入器
├── resources.qrc          Qt 资源文件
└── assets/GDUT_logo.png   开机 Logo
```

## 依赖

- Qt Widgets
- Linux V4L2 头文件：`linux/videodev2.h`
- Linux 系统调用：`ioctl`、`mmap`、`select`

## 常见问题

### 找不到摄像头

查看设备：

```sh
ls /dev/video*
```

指定正确节点：

```sh
./gdut_qt_camera --device /dev/video0
```

### 界面能打开但没有画面

可能原因：

1. 摄像头不支持 RGB565 或 YUYV。
2. 设备节点错误。
3. 当前用户没有访问 `/dev/videoX` 权限。

### 录像失败

Qt 录像使用 `QImage::save(..., "JPG")` 生成 MJPEG 帧。如果系统没有 JPEG 插件，录像会失败。

检查 Qt 插件目录是否包含：

```text
imageformats/libqjpeg.so
```

### 在 framebuffer Qt 环境运行

如果开发板没有桌面环境，Qt 通常需要指定平台插件，例如：

```sh
export QT_QPA_PLATFORM=linuxfb
./gdut_qt_camera --device /dev/video1
```

如果使用触摸屏，还需要根据开发板 Qt 环境配置触摸输入插件。
