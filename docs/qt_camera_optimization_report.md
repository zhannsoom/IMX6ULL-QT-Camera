# Qt 相机流畅性优化实验报告

## 1. 实验名称

基于 Qt Widgets 的嵌入式 Linux 相机实时预览卡顿与延迟优化。

## 2. 实验背景

原工程使用 framebuffer 直接绘制摄像头画面和功能栏，底部功能栏会被下一帧摄像头图像覆盖，表现为功能栏和视频画面快速交替闪烁。为解决 UI 闪屏问题，工程新增了 Qt 版本相机：

```text
qt_camera/
├── camerawindow.cpp      Qt 相机主界面
├── v4l2capture.cpp       V4L2 摄像头采集线程
├── aviwriter.cpp         MJPEG AVI 录像写入器
└── assets/GDUT_logo.png  开机 Logo
```

Qt 版本通过独立控件显示视频预览，并使用 Qt Widget 绘制顶部栏、底部功能栏、模式切换和快门按钮，解决了 framebuffer 版本 UI 被视频帧覆盖的问题。

但是在开发板上测试时，出现了新的问题：开机 Logo 页面显示正常，进入相机主界面后实时预览存在明显延迟和卡顿。

## 3. 实验环境

| 项目 | 内容 |
| --- | --- |
| 开发板 | I.MX6U/ARM Linux 开发板 |
| 系统接口 | V4L2、Qt Widgets、Linux framebuffer 或 X11 |
| 摄像头接口 | `/dev/videoX` |
| Qt 版本 | Qt 5.x |
| 工程目录 | `qt_camera/` |

## 4. 问题现象

进入相机界面后，出现以下现象：

1. 画面实时性差，手移动后屏幕画面明显滞后。
2. 预览帧率不稳定，有时连续卡住几帧。
3. 录像时卡顿更明显。
4. UI 控件显示正常，但视频预览跟手性差。

该问题和之前 framebuffer 版本的“功能栏闪屏”不是同一个问题：

| 问题 | 原因 |
| --- | --- |
| framebuffer 功能栏闪屏 | 摄像头帧直接覆盖同一块显存，把手动画出的 UI 擦掉 |
| Qt 版本预览延迟卡顿 | UI 线程处理帧速度低于采集速度，Qt 事件队列积压 |

## 5. 原始方案分析

Qt 版本初始数据流如下：

```text
V4L2 采集线程
        ↓ emit frameReady(QImage)
Qt 主线程 onFrameReady()
        ↓
保存当前帧
        ↓
QPixmap::fromImage()
        ↓
SmoothTransformation 缩放到屏幕大小
        ↓
QLabel::setPixmap()
        ↓
如果录像，则同步 JPEG 编码写 AVI
```

这个流程在 PC 上通常可以接受，但在 ARM 开发板上容易出现性能瓶颈。

### 5.1 问题一：Qt 事件队列积压

采集线程按照摄像头帧率不断发送：

```cpp
emit frameReady(image);
```

由于采集线程和 UI 线程不同，Qt 默认使用 queued connection。也就是说，每一帧都会变成一个排队等待 UI 线程处理的事件。

如果摄像头 30 fps，而开发板 UI 线程每秒只能处理 15 帧，就会出现：

```text
第 1 秒：积压 15 帧
第 2 秒：积压 30 帧
第 3 秒：积压 45 帧
```

此时屏幕显示的不是最新画面，而是在慢慢播放之前排队的旧帧，所以用户会感觉“延迟越来越大”。

实时相机和普通视频播放器不同。普通视频播放器可以缓冲，但实时相机应该优先显示最新帧。旧帧没有显示价值，应该丢弃。

### 5.2 问题二：实时预览使用高质量缩放

原预览缩放使用：

```cpp
Qt::SmoothTransformation
```

该模式画质较好，但计算量较大。实时预览每秒要缩放几十次，在 ARM 开发板上会造成明显 CPU 压力。

对于实时取景画面，流畅性比单帧缩放质量更重要。因此预览适合使用快速缩放：

```cpp
Qt::FastTransformation
```

相册和缩略图不是高频刷新，仍然可以使用高质量缩放。

### 5.3 问题三：录像 JPEG 编码阻塞 UI 线程

原方案中，录像帧写入也在 `onFrameReady()` 里执行：

```cpp
m_writer.addFrame(m_currentFrame);
```

`addFrame()` 内部会把当前图像压缩成 JPEG，再写入 AVI 文件。JPEG 编码是耗时操作，如果放在 UI 线程中执行，会直接阻塞界面刷新。

因此录像时卡顿更明显。

### 5.4 问题四：底部功能栏区域被视频重复绘制

原 Qt 预览控件覆盖整个窗口：

```cpp
m_previewLabel->setGeometry(rect);
```

底部功能栏虽然是独立控件，但视频预览仍然在底部栏下方每帧刷新，造成不必要的重绘和合成开销。

优化后预览控件只覆盖功能栏上方区域，底部栏不再被视频预览覆盖。

## 6. 优化目标

本次优化目标：

1. 相机预览尽量显示最新帧，不播放旧帧。
2. 防止 Qt 事件队列积压。
3. 降低每帧缩放绘制开销。
4. 录像时不阻塞 UI 线程。
5. 保持 iPhone 风格 UI 和开机 Logo 不变。
6. 不破坏原有拍照、录像、相册功能。

## 7. 优化方案

### 7.1 增加“最多一帧待显示”机制

在 `V4L2Capture` 中增加：

```cpp
std::atomic_bool m_framePending;
```

含义：

| 状态 | 含义 |
| --- | --- |
| `false` | UI 当前没有待处理帧，可以发送新帧 |
| `true` | 已经有一帧发给 UI 但还没处理完，新帧应丢弃 |

采集线程中修改为：

```cpp
if (buf.index < m_buffers.size() && m_buffers[buf.index].start &&
    !m_framePending.load()) {
    QImage image = convertFrame(...);
    if (!image.isNull() && !m_framePending.exchange(true))
        emit frameReady(image);
}
```

UI 线程处理完后调用：

```cpp
m_capture->notifyFrameConsumed();
```

对应函数：

```cpp
void V4L2Capture::notifyFrameConsumed()
{
    m_framePending.store(false);
}
```

这样就能保证：

```text
Qt 事件队列里最多只有 1 帧待显示
```

如果 UI 忙，采集线程会继续从 V4L2 取帧并归还缓冲，但不会把旧帧继续塞进 UI 队列。

这类策略叫“实时预览丢帧”。它不会降低实时性，反而能减少延迟。

### 7.2 去掉每帧深拷贝

原代码：

```cpp
m_currentFrame = image.copy();
```

优化后：

```cpp
m_currentFrame = image;
```

`QImage` 是隐式共享对象，赋值不会立即深拷贝像素数据。这样可以减少每帧一次完整图像内存复制。

对于 `800x480 RGB888`，一帧约：

```text
800 * 480 * 3 = 1.15 MB
```

如果每秒 30 帧，单纯深拷贝就可能产生几十 MB/s 的额外内存带宽消耗。

### 7.3 实时预览改用快速缩放

原代码：

```cpp
Qt::SmoothTransformation
```

优化后预览使用：

```cpp
Qt::FastTransformation
```

关键代码：

```cpp
m_previewLabel->setPixmap(scaledToFill(m_currentFrame,
                                       m_previewLabel->size(),
                                       Qt::FastTransformation));
```

同时保留相册和缩略图的高质量缩放：

```cpp
Qt::SmoothTransformation
```

原因：

| 场景 | 刷新频率 | 缩放策略 |
| --- | --- | --- |
| 实时预览 | 每秒多次 | 快速缩放，优先流畅 |
| 相册图片 | 用户切换时才刷新 | 高质量缩放，优先画质 |
| 拍照缩略图 | 拍照后刷新一次 | 高质量缩放 |

### 7.4 预览区域避开底部功能栏

原代码：

```cpp
m_previewLabel->setGeometry(rect);
```

优化后：

```cpp
int bottomHeight = qMax(118, rect.height() / 5);
m_previewLabel->setGeometry(0, 0, rect.width(),
                            rect.height() - bottomHeight);
```

这样每帧只刷新上方预览区域，底部功能栏保持独立绘制。

优点：

1. 减少每帧绘制面积。
2. 减少透明控件合成压力。
3. 功能栏视觉更稳定。

### 7.5 录像编码移动到后台线程

新增 `RecordingWorker`：

```cpp
class RecordingWorker : public QObject
{
    Q_OBJECT

public slots:
    bool start(const QString &filePath, int width, int height, int fps);
    void addFrame(const QImage &image);
    void stop();

signals:
    void frameWritten(bool ok);

private:
    AviWriter m_writer;
};
```

在 `CameraWindow` 中创建后台线程：

```cpp
m_recordWorker = new RecordingWorker;
m_recordWorker->moveToThread(&m_recordThread);
m_recordThread.start();
```

录像时，UI 线程只负责把帧投递给后台线程：

```cpp
QMetaObject::invokeMethod(m_recordWorker, "addFrame",
                          Qt::QueuedConnection,
                          Q_ARG(QImage, m_currentFrame));
```

真正耗时的 JPEG 编码和 AVI 写入在后台线程完成。

### 7.6 限制录像队列最多积压一帧

仅仅把录像移到后台线程还不够。如果后台编码速度低于投递速度，录像线程队列也会积压。因此增加：

```cpp
bool m_recordFramePending = false;
```

只有上一帧写完后，才允许投递下一帧：

```cpp
if ((!m_recordFrameTimer.isValid() ||
     m_recordFrameTimer.elapsed() >= interval) &&
    !m_recordFramePending) {
    m_recordFramePending = true;
    QMetaObject::invokeMethod(m_recordWorker, "addFrame",
                              Qt::QueuedConnection,
                              Q_ARG(QImage, m_currentFrame));
    m_recordFrameTimer.restart();
}
```

后台写完后通知 UI：

```cpp
emit frameWritten(m_writer.addFrame(image));
```

UI 收到后：

```cpp
m_recordFramePending = false;
```

这样录像不会无限积压旧帧。

## 8. 修改文件

本次优化修改了以下文件：

| 文件 | 修改内容 |
| --- | --- |
| `qt_camera/v4l2capture.h` | 增加 `notifyFrameConsumed()` 和 `m_framePending` |
| `qt_camera/v4l2capture.cpp` | 增加预览帧丢帧机制，防止 UI 队列积压 |
| `qt_camera/camerawindow.h` | 增加 `RecordingWorker`、录像线程和录像状态变量 |
| `qt_camera/camerawindow.cpp` | 优化预览缩放、去掉深拷贝、缩小预览刷新区域、录像后台化 |
| `qt_camera/README_QT_CAMERA.md` | 补充流畅性优化说明 |
| `docs/qt_camera_optimization_report.md` | 新增本实验报告 |

## 9. 优化后的数据流

优化后的实时预览链路：

```text
V4L2 采集线程
        ↓
判断 UI 是否已有待处理帧
        ↓
如果没有，转换当前帧并发送到 UI
        ↓
如果有，丢弃当前帧并立即归还 V4L2 缓冲
        ↓
UI 线程快速缩放并显示最新帧
        ↓
UI 通知采集线程可以发送下一帧
```

优化后的录像链路：

```text
UI 线程收到最新帧
        ↓
判断是否达到录像帧间隔
        ↓
判断录像线程是否已有待写帧
        ↓
投递一帧到 RecordingWorker
        ↓
后台线程 JPEG 编码并写 AVI
        ↓
写完后通知 UI 可投递下一帧
```

## 10. 预期效果

优化后应达到以下效果：

1. 进入相机后画面延迟明显降低。
2. 快速移动摄像头或手掌时，画面更接近实时。
3. UI 操作响应更快。
4. 底部功能栏不闪烁，也不被视频帧覆盖。
5. 录像时预览不再因为 JPEG 编码明显卡住。

## 11. 编译和运行

在交叉编译 SDK 环境中：

```sh
source /opt/fsl-imx-x11/4.1.15-2.1.0/environment-setup-cortexa7hf-neon-poky-linux-gnueabi
cd ~/project/v4l2/v4l2_camera/qt_camera
make distclean
qmake qt_camera.pro
make -j$(nproc)
file imx6ull_qt_camera
```

确认输出为 ARM 程序：

```text
ELF 32-bit LSB executable, ARM
```

拷贝到开发板：

```sh
scp imx6ull_qt_camera root@开发板IP:/home/root/
```

开发板运行：

```sh
cd /home/root
chmod +x imx6ull_qt_camera
export QT_QPA_PLATFORM=linuxfb
./imx6ull_qt_camera --device /dev/video1 --width 800 --height 480
```

如果摄像头是 `/dev/video0`：

```sh
./imx6ull_qt_camera --device /dev/video0 --width 800 --height 480
```

## 12. 验证方法

### 12.1 主观验证

1. 进入相机后左右移动摄像头，观察画面是否拖影或明显滞后。
2. 在摄像头前快速挥手，观察屏幕是否接近实时。
3. 切换 PHOTO/VIDEO 模式，观察 UI 是否卡顿。
4. 开始录像，观察预览是否仍然流畅。
5. 打开相册再返回相机，观察预览是否恢复正常。

### 12.2 命令行验证

检查程序架构：

```sh
file imx6ull_qt_camera
```

检查 CPU 占用：

```sh
top
```

如果 CPU 占用仍然很高，可以降低输入分辨率：

```sh
./imx6ull_qt_camera --device /dev/video1 --width 640 --height 480
```

或者降低摄像头帧率：

```sh
./imx6ull_qt_camera --device /dev/video1 --width 800 --height 480 --fps 15
```

## 13. 后续可继续优化方向

如果开发板性能仍然不足，可以继续优化：

1. 使用摄像头直接输出与屏幕一致的 RGB565，减少 YUYV 转 RGB 的 CPU 开销。
2. 使用自定义 `QWidget::paintEvent()` 绘制预览，减少 QLabel/QPixmap 中间转换。
3. 使用硬件加速显示接口，例如 DRM/KMS、OpenGL ES 或 G2D。
4. 录像改用硬件编码器或 V4L2 M2M 编码。
5. 在低性能板子上默认使用 `640x480@15fps`。

## 14. 实验总结

本次实验解决的核心问题不是摄像头采集失败，而是实时视频 UI 程序中常见的“生产者速度大于消费者速度”问题。

摄像头采集线程是生产者，Qt UI 线程是消费者。如果生产者持续把所有帧都送给消费者，而消费者处理不过来，就会造成帧队列积压，最终表现为画面延迟和卡顿。

优化的关键思想是：

```text
实时预览只需要最新帧，不需要旧帧排队。
```

因此，本次优化通过丢弃过期帧、快速缩放、减少绘制区域和后台录像编码，使 Qt 相机从“尽量显示每一帧”改为“尽量实时显示最新帧”。这更符合相机取景器的实际需求。
