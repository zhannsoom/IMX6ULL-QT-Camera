#ifndef V4L2CAPTURE_H
#define V4L2CAPTURE_H

#include <QImage>
#include <QMutex>
#include <QThread>
#include <QString>

#include <atomic>
#include <vector>

class V4L2Capture : public QThread
{
    Q_OBJECT

public:
    explicit V4L2Capture(QObject *parent = nullptr);
    ~V4L2Capture() override;

    void configure(const QString &device, int width, int height, int fps);
    void stop();
    void notifyFrameConsumed();

signals:
    void frameReady(const QImage &image);
    void cameraError(const QString &message);
    void cameraInfo(const QString &message);

protected:
    void run() override;

private:
    struct Buffer {
        void *start = nullptr;
        size_t length = 0;
    };

    bool openDevice();
    bool setFormat();
    bool initMmap();
    bool startStream();
    void captureLoop();
    void stopStream();
    void uninitMmap();
    void closeDevice();

    QImage convertFrame(const unsigned char *data, int bytesUsed) const;
    QImage convertRgb565(const unsigned char *data) const;
    QImage convertYuyv(const unsigned char *data) const;
    static int clampColor(int value);

    QString m_device;
    int m_requestWidth = 800;
    int m_requestHeight = 480;
    int m_requestFps = 30;

    int m_fd = -1;
    int m_width = 0;
    int m_height = 0;
    unsigned int m_pixelFormat = 0;
    std::vector<Buffer> m_buffers;
    std::atomic_bool m_running;
    std::atomic_bool m_framePending;
};

#endif
