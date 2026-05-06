#ifndef CAMERAWINDOW_H
#define CAMERAWINDOW_H

#include "aviwriter.h"
#include "v4l2capture.h"

#include <QElapsedTimer>
#include <QImage>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QStackedWidget>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QWidget>

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

class CameraWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit CameraWindow(const QString &device, int width, int height, int fps,
                          QWidget *parent = nullptr);
    ~CameraWindow() override;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void enterCamera();
    void onFrameReady(const QImage &image);
    void onCameraError(const QString &message);
    void onCameraInfo(const QString &message);
    void takePhoto();
    void toggleRecording();
    void switchToPhotoMode();
    void switchToVideoMode();
    void openAlbum();
    void closeAlbum();
    void nextAlbum();
    void previousAlbum();
    void updateRecordUi();

private:
    enum CaptureMode {
        PhotoMode,
        VideoMode
    };

    void buildSplashPage();
    void buildCameraPage();
    void buildAlbumPage();
    void layoutCameraPage();
    void layoutAlbumPage();
    void updatePreview();
    void updateModeUi();
    void updateThumbnail(const QImage &image);
    void refreshAlbum();
    void showAlbumImage();
    void stopRecording();

    QString makeTimestampPath(const QString &dirName,
                              const QString &prefix,
                              const QString &suffix) const;
    static bool ensureDir(const QString &dirName);
    static QPixmap scaledToFill(const QImage &image, const QSize &size,
                                Qt::TransformationMode mode);
    static QPixmap scaledToFit(const QImage &image, const QSize &size);

    QString m_device;
    int m_requestWidth = 800;
    int m_requestHeight = 480;
    int m_requestFps = 30;
    int m_recordFps = 15;

    QStackedWidget *m_stack = nullptr;
    QWidget *m_splashPage = nullptr;
    QWidget *m_cameraPage = nullptr;
    QWidget *m_albumPage = nullptr;

    QLabel *m_splashLogo = nullptr;
    QLabel *m_previewLabel = nullptr;
    QWidget *m_topBar = nullptr;
    QWidget *m_modeBar = nullptr;
    QWidget *m_bottomBar = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_deviceLabel = nullptr;
    QLabel *m_recordLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_closeButton = nullptr;
    QPushButton *m_albumButton = nullptr;
    QPushButton *m_shutterButton = nullptr;
    QPushButton *m_photoModeButton = nullptr;
    QPushButton *m_videoModeButton = nullptr;

    QWidget *m_albumTopBar = nullptr;
    QLabel *m_albumTitle = nullptr;
    QLabel *m_albumImage = nullptr;
    QWidget *m_albumBottomBar = nullptr;
    QPushButton *m_albumBackButton = nullptr;
    QPushButton *m_albumPrevButton = nullptr;
    QPushButton *m_albumNextButton = nullptr;

    V4L2Capture *m_capture = nullptr;
    RecordingWorker *m_recordWorker = nullptr;
    QThread m_recordThread;
    QTimer m_recordUiTimer;
    QElapsedTimer m_recordTimer;
    QElapsedTimer m_recordFrameTimer;

    QImage m_currentFrame;
    CaptureMode m_mode = PhotoMode;
    bool m_recording = false;
    bool m_recordFramePending = false;
    QString m_recordPath;
    QStringList m_albumFiles;
    int m_albumIndex = -1;
};

#endif
