#include "v4l2capture.h"

#include <QDebug>

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>

namespace {

constexpr int BufferCount = 4;

int xioctl(int fd, unsigned long request, void *arg)
{
    int ret = 0;
    do {
        ret = ioctl(fd, request, arg);
    } while (ret == -1 && errno == EINTR);
    return ret;
}

QString errnoText(const char *prefix)
{
    return QStringLiteral("%1: %2").arg(QString::fromLatin1(prefix),
                                        QString::fromLocal8Bit(strerror(errno)));
}

} // namespace

V4L2Capture::V4L2Capture(QObject *parent)
    : QThread(parent),
      m_running(false),
      m_framePending(false)
{
}

V4L2Capture::~V4L2Capture()
{
    stop();
    wait(1500);
}

void V4L2Capture::configure(const QString &device, int width, int height, int fps)
{
    m_device = device;
    m_requestWidth = width;
    m_requestHeight = height;
    m_requestFps = fps;
}

void V4L2Capture::stop()
{
    m_running.store(false);
}

void V4L2Capture::notifyFrameConsumed()
{
    m_framePending.store(false);
}

void V4L2Capture::run()
{
    m_running.store(true);
    m_framePending.store(false);

    if (!openDevice() || !setFormat() || !initMmap() || !startStream()) {
        stopStream();
        uninitMmap();
        closeDevice();
        m_running.store(false);
        return;
    }

    emit cameraInfo(QStringLiteral("Camera started: %1, %2x%3")
                        .arg(m_device)
                        .arg(m_width)
                        .arg(m_height));
    captureLoop();

    stopStream();
    uninitMmap();
    closeDevice();
    m_running.store(false);
}

bool V4L2Capture::openDevice()
{
    m_fd = open(m_device.toLocal8Bit().constData(), O_RDWR | O_NONBLOCK);
    if (m_fd < 0) {
        emit cameraError(errnoText("open camera failed"));
        return false;
    }

    v4l2_capability cap;
    memset(&cap, 0, sizeof(cap));
    if (xioctl(m_fd, VIDIOC_QUERYCAP, &cap) < 0) {
        emit cameraError(errnoText("VIDIOC_QUERYCAP failed"));
        return false;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        emit cameraError(QStringLiteral("%1 is not a video capture device").arg(m_device));
        return false;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        emit cameraError(QStringLiteral("%1 does not support streaming mmap").arg(m_device));
        return false;
    }

    return true;
}

bool V4L2Capture::setFormat()
{
    v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = static_cast<__u32>(m_requestWidth);
    fmt.fmt.pix.height = static_cast<__u32>(m_requestHeight);
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;

    if (xioctl(m_fd, VIDIOC_S_FMT, &fmt) < 0 ||
        fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB565) {
        memset(&fmt, 0, sizeof(fmt));
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = static_cast<__u32>(m_requestWidth);
        fmt.fmt.pix.height = static_cast<__u32>(m_requestHeight);
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        fmt.fmt.pix.field = V4L2_FIELD_ANY;
        if (xioctl(m_fd, VIDIOC_S_FMT, &fmt) < 0 ||
            fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV) {
            emit cameraError(QStringLiteral("camera does not support RGB565 or YUYV"));
            return false;
        }
    }

    m_width = static_cast<int>(fmt.fmt.pix.width);
    m_height = static_cast<int>(fmt.fmt.pix.height);
    m_pixelFormat = fmt.fmt.pix.pixelformat;

    v4l2_streamparm parm;
    memset(&parm, 0, sizeof(parm));
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(m_fd, VIDIOC_G_PARM, &parm) == 0 &&
        (parm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME)) {
        parm.parm.capture.timeperframe.numerator = 1;
        parm.parm.capture.timeperframe.denominator = static_cast<__u32>(m_requestFps);
        xioctl(m_fd, VIDIOC_S_PARM, &parm);
    }

    return true;
}

bool V4L2Capture::initMmap()
{
    v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = BufferCount;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(m_fd, VIDIOC_REQBUFS, &req) < 0) {
        emit cameraError(errnoText("VIDIOC_REQBUFS failed"));
        return false;
    }

    if (req.count < 2) {
        emit cameraError(QStringLiteral("not enough V4L2 buffers"));
        return false;
    }

    m_buffers.resize(req.count);
    for (unsigned int i = 0; i < req.count; ++i) {
        v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (xioctl(m_fd, VIDIOC_QUERYBUF, &buf) < 0) {
            emit cameraError(errnoText("VIDIOC_QUERYBUF failed"));
            return false;
        }

        m_buffers[i].length = buf.length;
        m_buffers[i].start = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE,
                                  MAP_SHARED, m_fd, buf.m.offset);
        if (m_buffers[i].start == MAP_FAILED) {
            m_buffers[i].start = nullptr;
            emit cameraError(errnoText("mmap camera buffer failed"));
            return false;
        }
    }

    for (unsigned int i = 0; i < m_buffers.size(); ++i) {
        v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (xioctl(m_fd, VIDIOC_QBUF, &buf) < 0) {
            emit cameraError(errnoText("VIDIOC_QBUF failed"));
            return false;
        }
    }

    return true;
}

bool V4L2Capture::startStream()
{
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(m_fd, VIDIOC_STREAMON, &type) < 0) {
        emit cameraError(errnoText("VIDIOC_STREAMON failed"));
        return false;
    }
    return true;
}

void V4L2Capture::captureLoop()
{
    while (m_running.load()) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(m_fd, &fds);

        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 200000;

        int ret = select(m_fd + 1, &fds, nullptr, nullptr, &tv);
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            emit cameraError(errnoText("select camera failed"));
            break;
        }
        if (ret == 0)
            continue;

        v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (xioctl(m_fd, VIDIOC_DQBUF, &buf) < 0) {
            if (errno == EAGAIN)
                continue;
            emit cameraError(errnoText("VIDIOC_DQBUF failed"));
            break;
        }

        if (buf.index < m_buffers.size() && m_buffers[buf.index].start &&
            !m_framePending.load()) {
            QImage image = convertFrame(static_cast<unsigned char *>(m_buffers[buf.index].start),
                                        static_cast<int>(buf.bytesused));
            if (!image.isNull() && !m_framePending.exchange(true))
                emit frameReady(image);
        }

        if (xioctl(m_fd, VIDIOC_QBUF, &buf) < 0) {
            emit cameraError(errnoText("VIDIOC_QBUF failed"));
            break;
        }
    }
}

void V4L2Capture::stopStream()
{
    if (m_fd < 0)
        return;

    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(m_fd, VIDIOC_STREAMOFF, &type);
}

void V4L2Capture::uninitMmap()
{
    for (Buffer &buffer : m_buffers) {
        if (buffer.start && buffer.length > 0)
            munmap(buffer.start, buffer.length);
        buffer.start = nullptr;
        buffer.length = 0;
    }
    m_buffers.clear();
}

void V4L2Capture::closeDevice()
{
    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }
}

QImage V4L2Capture::convertFrame(const unsigned char *data, int bytesUsed) const
{
    Q_UNUSED(bytesUsed);

    if (m_pixelFormat == V4L2_PIX_FMT_RGB565)
        return convertRgb565(data);
    if (m_pixelFormat == V4L2_PIX_FMT_YUYV)
        return convertYuyv(data);
    return QImage();
}

QImage V4L2Capture::convertRgb565(const unsigned char *data) const
{
    QImage image(m_width, m_height, QImage::Format_RGB888);
    for (int y = 0; y < m_height; ++y) {
        const quint16 *src = reinterpret_cast<const quint16 *>(data + y * m_width * 2);
        uchar *dst = image.scanLine(y);
        for (int x = 0; x < m_width; ++x) {
            quint16 p = src[x];
            int r = (p >> 11) & 0x1F;
            int g = (p >> 5) & 0x3F;
            int b = p & 0x1F;
            dst[x * 3 + 0] = static_cast<uchar>((r << 3) | (r >> 2));
            dst[x * 3 + 1] = static_cast<uchar>((g << 2) | (g >> 4));
            dst[x * 3 + 2] = static_cast<uchar>((b << 3) | (b >> 2));
        }
    }
    return image;
}

QImage V4L2Capture::convertYuyv(const unsigned char *data) const
{
    QImage image(m_width, m_height, QImage::Format_RGB888);
    for (int y = 0; y < m_height; ++y) {
        const unsigned char *src = data + y * m_width * 2;
        uchar *dst = image.scanLine(y);
        for (int x = 0; x < m_width; x += 2) {
            int y0 = src[0];
            int u = src[1] - 128;
            int y1 = src[2];
            int v = src[3] - 128;

            int r0 = y0 + ((359 * v) >> 8);
            int g0 = y0 - ((88 * u + 183 * v) >> 8);
            int b0 = y0 + ((454 * u) >> 8);
            int r1 = y1 + ((359 * v) >> 8);
            int g1 = y1 - ((88 * u + 183 * v) >> 8);
            int b1 = y1 + ((454 * u) >> 8);

            dst[0] = static_cast<uchar>(clampColor(r0));
            dst[1] = static_cast<uchar>(clampColor(g0));
            dst[2] = static_cast<uchar>(clampColor(b0));
            if (x + 1 < m_width) {
                dst[3] = static_cast<uchar>(clampColor(r1));
                dst[4] = static_cast<uchar>(clampColor(g1));
                dst[5] = static_cast<uchar>(clampColor(b1));
            }

            src += 4;
            dst += 6;
        }
    }
    return image;
}

int V4L2Capture::clampColor(int value)
{
    if (value < 0)
        return 0;
    if (value > 255)
        return 255;
    return value;
}
