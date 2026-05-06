# IMX6ULL-QT-Camera

这是一个面向 I.MX6ULL / ARM Linux 开发板的嵌入式相机项目。项目从最基础的 C 语言 framebuffer 版本开始，实现摄像头预览、触摸操作、拍照、简单录像和相册浏览；后续又增加了 Qt Widgets 版本，用来解决原 framebuffer 版本 UI 容易被视频帧覆盖、界面闪烁和预览卡顿的问题。

这个仓库主要用于记录我对嵌入式 Linux 相机链路的学习和实践，不是直接面向量产的完整相机应用。重点是把摄像头采集、显示、触摸交互和简单文件保存这些环节跑通，并理解它们之间的数据流和常见问题。

## 与简历项目对应

这个仓库主要对应我简历里的两个项目经历：

- **I.MX6ULL 嵌入式 Qt 相机系统**：对应 `qt_camera/` 目录，重点是 Qt 界面、V4L2 采集线程、拍照录像和预览流畅性优化。
- **V4L2 + framebuffer 触摸 UI 相机**：对应根目录下的 C 语言版本，重点是 framebuffer 显示、触摸事件读取、V4L2 取帧和基础相机功能。

## 我做了什么

- 使用 C 语言实现了基于 V4L2 + framebuffer + input event 的基础相机程序。
- 使用 `ioctl`、`mmap`、`VIDIOC_REQBUFS`、`VIDIOC_QBUF`、`VIDIOC_DQBUF` 等接口完成摄像头采集流程。
- 通过 `/dev/input/eventX` 读取触摸屏事件，实现拍照、录像、相册、返回等基础交互。
- 增加 Qt Widgets 版本，把视频预览和功能按钮拆成独立控件，解决原来直接画 framebuffer 时功能栏被视频帧覆盖的问题。
- 针对 ARM 板端预览延迟，加入“最多一帧待显示”的思路，避免旧帧在 Qt 事件队列中越积越多。
- 整理了 Qt 版本的编译、运行和问题分析文档，方便复现和继续优化。

## 项目结构

```text
.
├── main.c                  C 版本程序入口
├── Makefile                C 版本编译脚本
├── lcd/                    framebuffer 初始化和显存访问
├── v4l2/                   摄像头初始化、格式设置、取帧主循环
├── input/                  触摸屏事件读取
├── ui/                     基础 UI 绘制和按钮命中判断
├── photo/                  BMP 拍照保存
├── video/                  RGB565 原始帧录像
├── album/                  BMP 相册浏览
├── qt_camera/              Qt Widgets 相机版本
└── docs/                   项目问题分析和优化记录
```

## 两个版本的区别

| 版本 | 主要技术 | 解决的问题 | 我的理解 |
| --- | --- | --- | --- |
| C / framebuffer 版本 | V4L2、framebuffer、input event | 跑通摄像头采集、LCD 显示和触摸操作 | 适合理解底层数据流，但 UI 和视频都写同一块显存，容易互相覆盖 |
| Qt Widgets 版本 | V4L2、QThread、Qt Widgets | 改善界面闪烁、提升交互体验 | 视频预览和按钮控件分开管理，结构更清晰，也更容易维护 |

## 核心流程

### 1. 摄像头采集

V4L2 采集的大致流程如下：

```text
打开 /dev/videoX
        ↓
查询设备能力 VIDIOC_QUERYCAP
        ↓
设置图像格式 VIDIOC_S_FMT
        ↓
申请 mmap 缓冲区 VIDIOC_REQBUFS
        ↓
缓冲区入队 VIDIOC_QBUF
        ↓
开启采集 VIDIOC_STREAMON
        ↓
循环取帧 VIDIOC_DQBUF / 归还 VIDIOC_QBUF
```

这部分是整个项目的基础。只有把摄像头格式、缓冲区和取帧流程处理正确，后面的预览、拍照和录像才有稳定的数据来源。

### 2. framebuffer 显示

C 版本中，摄像头帧会直接拷贝到 LCD framebuffer。这个方式比较直接，也方便理解显存显示原理，但缺点是 UI 按钮也是画在同一块显存上。下一帧视频到来时，如果直接覆盖整屏，就可能把刚画好的功能栏擦掉。

这也是我后面增加 Qt 版本的原因。

### 3. Qt 版本优化

Qt 版本中，视频帧显示在预览控件里，顶部栏、底部栏、按钮等 UI 由 Qt 控件管理。这样视频刷新不会直接擦掉按钮。

在开发板上测试时，实时预览还会遇到延迟问题。我理解的原因是：采集线程产生帧的速度可能比 UI 线程显示帧的速度快，如果每一帧都排队显示，队列里会堆积旧帧，屏幕看到的就不是最新画面。

所以 Qt 版本中加入了一个简单策略：

```text
UI 正在处理上一帧时，新来的旧帧可以丢弃
实时预览优先显示最新帧，而不是把每一帧都慢慢播放完
```

详细过程记录在：

- [Qt 相机流畅性优化实验报告](docs/qt_camera_optimization_report.md)
- [Qt 相机版本说明](qt_camera/README_QT_CAMERA.md)

## 已实现功能

- 摄像头实时预览
- 触摸屏按钮操作
- 拍照保存 BMP / JPG / PNG
- 简单录像保存 RGB565 原始帧或 MJPEG AVI
- 相册浏览
- 摄像头设备节点参数配置
- Qt 窗口模式和开发板全屏模式

## 编译运行

### C 版本

交叉编译：

```sh
make clean
make
```

如果在开发板本机编译：

```sh
make clean
make CROSS_COMPILE=
```

运行：

```sh
CAM_DEV=/dev/video1 TOUCH_DEV=/dev/input/event0 ./v4l2_test
```

### Qt 版本

进入 Qt 工程目录：

```sh
cd qt_camera
qmake qt_camera.pro
make
```

运行：

```sh
./imx6ull_qt_camera --device /dev/video1 --width 800 --height 480
```

如果开发板使用 framebuffer Qt 环境：

```sh
export QT_QPA_PLATFORM=linuxfb
./imx6ull_qt_camera --device /dev/video1 --width 800 --height 480
```

## 运行环境

- I.MX6ULL / ARM Linux 开发板
- Linux framebuffer：如 `/dev/fb0`
- V4L2 摄像头：如 `/dev/video0` 或 `/dev/video1`
- Linux input 触摸屏：如 `/dev/input/event0`
- GCC / ARM 交叉编译工具链
- Qt 5.x（Qt 版本需要）

## 项目收获

通过这个项目，我主要加深了下面几方面的理解：

- V4L2 不是简单读取一个文件，而是要正确配置格式、申请缓冲区、取帧和归还缓冲区。
- framebuffer 显示方式直观，但 UI 和视频共用显存时，需要特别注意绘制顺序和覆盖问题。
- 实时预览更关注“最新画面”，不是每一帧都必须显示，适当丢弃旧帧反而能减少延迟。
- 嵌入式项目不仅要写功能，还要考虑开发板性能、设备节点差异、图像格式兼容和部署复现。

## 后续可优化方向

- 增加更多摄像头格式兼容，例如 MJPEG。
- 增加触摸坐标校准。
- 增加更标准的视频封装和播放功能。
- 在开发板上进一步测试不同分辨率、帧率下的流畅度。
- 尝试使用硬件加速显示或硬件编码，降低 CPU 占用。
