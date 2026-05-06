#include "camerawindow.h"

#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QFileInfoList>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QMetaObject>
#include <QMetaType>
#include <QPainter>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QVBoxLayout>

bool RecordingWorker::start(const QString &filePath, int width, int height, int fps)
{
    return m_writer.start(filePath, width, height, fps);
}

void RecordingWorker::addFrame(const QImage &image)
{
    emit frameWritten(m_writer.addFrame(image));
}

void RecordingWorker::stop()
{
    m_writer.stop();
}

namespace {

const char *PanelStyle =
    "background-color: rgba(0, 0, 0, 205);"
    "color: white;";

const char *GlassStyle =
    "background-color: rgba(0, 0, 0, 132);"
    "border: 1px solid rgba(255, 255, 255, 42);"
    "border-radius: 18px;"
    "color: white;";

QString textButtonStyle(bool active)
{
    return QStringLiteral(
        "QPushButton {"
        "  color: %1;"
        "  background: transparent;"
        "  border: none;"
        "  font-size: 18px;"
        "  font-weight: 700;"
        "  padding: 8px 14px;"
        "}"
        "QPushButton:pressed { color: #ffffff; }")
        .arg(active ? QStringLiteral("#ffd44d") : QStringLiteral("#f5f5f7"));
}

QString pillButtonStyle()
{
    return QStringLiteral(
        "QPushButton {"
        "  color: white;"
        "  background-color: rgba(255, 255, 255, 38);"
        "  border: 1px solid rgba(255, 255, 255, 70);"
        "  border-radius: 20px;"
        "  font-size: 16px;"
        "  font-weight: 700;"
        "}"
        "QPushButton:pressed { background-color: rgba(255, 255, 255, 72); }");
}

QString shutterStyle(bool videoMode)
{
    if (videoMode) {
        return QStringLiteral(
            "QPushButton {"
            "  background-color: #ff2d55;"
            "  border: 6px solid white;"
            "  border-radius: 38px;"
            "}"
            "QPushButton:pressed { background-color: #c9183f; }");
    }

    return QStringLiteral(
        "QPushButton {"
        "  background-color: white;"
        "  border: 6px solid rgba(255, 255, 255, 160);"
        "  border-radius: 38px;"
        "}"
        "QPushButton:pressed { background-color: #d8d8dc; }");
}

QString recordTimeText(qint64 elapsedMs)
{
    qint64 totalSeconds = elapsedMs / 1000;
    qint64 minutes = totalSeconds / 60;
    qint64 seconds = totalSeconds % 60;
    return QStringLiteral("REC %1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

} // namespace

CameraWindow::CameraWindow(const QString &device, int width, int height, int fps,
                           QWidget *parent)
    : QMainWindow(parent),
      m_device(device),
      m_requestWidth(width),
      m_requestHeight(height),
      m_requestFps(fps)
{
    setWindowTitle(QStringLiteral("IMX6ULL-QT-Camera"));
    setMinimumSize(640, 360);
    qRegisterMetaType<QImage>("QImage");

    m_stack = new QStackedWidget(this);
    setCentralWidget(m_stack);

    buildSplashPage();
    buildCameraPage();
    buildAlbumPage();

    m_recordWorker = new RecordingWorker;
    m_recordWorker->moveToThread(&m_recordThread);
    connect(&m_recordThread, &QThread::finished,
            m_recordWorker, &QObject::deleteLater);
    connect(m_recordWorker, &RecordingWorker::frameWritten, this,
            [this](bool ok) {
                m_recordFramePending = false;
                if (!ok && m_recording) {
                    m_statusLabel->setText(QStringLiteral("Record failed: JPEG plugin missing or disk error"));
                    m_statusLabel->show();
                    stopRecording();
                }
            });
    m_recordThread.start();

    connect(&m_recordUiTimer, &QTimer::timeout, this, &CameraWindow::updateRecordUi);

    m_stack->setCurrentWidget(m_splashPage);
    QTimer::singleShot(1700, this, &CameraWindow::enterCamera);
}

CameraWindow::~CameraWindow()
{
    stopRecording();
    m_recordThread.quit();
    m_recordThread.wait(1500);
    if (m_capture) {
        m_capture->stop();
        m_capture->wait(1500);
    }
}

void CameraWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    layoutCameraPage();
    layoutAlbumPage();
    updatePreview();
    showAlbumImage();
}

void CameraWindow::closeEvent(QCloseEvent *event)
{
    stopRecording();
    if (m_capture)
        m_capture->stop();
    QMainWindow::closeEvent(event);
}

void CameraWindow::buildSplashPage()
{
    m_splashPage = new QWidget;
    m_splashPage->setStyleSheet("background-color: #000000;");
    QVBoxLayout *layout = new QVBoxLayout(m_splashPage);
    layout->setContentsMargins(42, 42, 42, 42);
    layout->addStretch();

    m_splashLogo = new QLabel;
    m_splashLogo->setAlignment(Qt::AlignCenter);
    m_splashLogo->setPixmap(QPixmap(":/assets/GDUT_logo.png")
                                .scaled(760, 280, Qt::KeepAspectRatio,
                                        Qt::SmoothTransformation));
    layout->addWidget(m_splashLogo, 0, Qt::AlignCenter);

    QLabel *title = new QLabel(QStringLiteral("IMX6ULL QT CAMERA"));
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("color: #d20a24; font-size: 28px; font-weight: 800; letter-spacing: 2px;");
    layout->addSpacing(28);
    layout->addWidget(title);
    layout->addStretch();
    m_stack->addWidget(m_splashPage);

    QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect(m_splashLogo);
    m_splashLogo->setGraphicsEffect(effect);
    QPropertyAnimation *animation = new QPropertyAnimation(effect, "opacity", m_splashLogo);
    animation->setDuration(1300);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void CameraWindow::buildCameraPage()
{
    m_cameraPage = new QWidget;
    m_cameraPage->setStyleSheet("background-color: #000000;");

    m_previewLabel = new QLabel(m_cameraPage);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet("background-color: #000000; color: #9ca3af; font-size: 18px;");
    m_previewLabel->setText(QStringLiteral("Starting camera..."));

    m_topBar = new QWidget(m_cameraPage);
    m_topBar->setStyleSheet(GlassStyle);
    QHBoxLayout *topLayout = new QHBoxLayout(m_topBar);
    topLayout->setContentsMargins(18, 8, 18, 8);
    topLayout->setSpacing(12);

    m_recordLabel = new QLabel(QStringLiteral("READY"));
    m_recordLabel->setStyleSheet("color: #b8c0cc; font-size: 15px; font-weight: 700;");
    m_titleLabel = new QLabel(QStringLiteral("IMX6ULL Camera"));
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet("color: white; font-size: 20px; font-weight: 800;");
    m_deviceLabel = new QLabel(m_device);
    m_deviceLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_deviceLabel->setStyleSheet("color: #c8ccd2; font-size: 14px;");
    m_closeButton = new QPushButton(QStringLiteral("X"));
    m_closeButton->setFixedSize(42, 42);
    m_closeButton->setStyleSheet(pillButtonStyle());
    connect(m_closeButton, &QPushButton::clicked, this, &CameraWindow::close);

    topLayout->addWidget(m_recordLabel);
    topLayout->addStretch();
    topLayout->addWidget(m_titleLabel);
    topLayout->addStretch();
    topLayout->addWidget(m_deviceLabel);
    topLayout->addWidget(m_closeButton);

    m_modeBar = new QWidget(m_cameraPage);
    m_modeBar->setStyleSheet("background-color: rgba(0, 0, 0, 95); border-radius: 22px;");
    QHBoxLayout *modeLayout = new QHBoxLayout(m_modeBar);
    modeLayout->setContentsMargins(8, 2, 8, 2);
    modeLayout->setSpacing(12);
    m_photoModeButton = new QPushButton(QStringLiteral("PHOTO"));
    m_videoModeButton = new QPushButton(QStringLiteral("VIDEO"));
    modeLayout->addWidget(m_photoModeButton);
    modeLayout->addWidget(m_videoModeButton);
    connect(m_photoModeButton, &QPushButton::clicked, this, &CameraWindow::switchToPhotoMode);
    connect(m_videoModeButton, &QPushButton::clicked, this, &CameraWindow::switchToVideoMode);

    m_bottomBar = new QWidget(m_cameraPage);
    m_bottomBar->setStyleSheet(PanelStyle);
    QHBoxLayout *bottomLayout = new QHBoxLayout(m_bottomBar);
    bottomLayout->setContentsMargins(32, 18, 32, 22);
    bottomLayout->setSpacing(34);

    m_albumButton = new QPushButton(QStringLiteral("ALBUM"));
    m_albumButton->setFixedSize(96, 72);
    m_albumButton->setStyleSheet(pillButtonStyle());
    connect(m_albumButton, &QPushButton::clicked, this, &CameraWindow::openAlbum);

    m_shutterButton = new QPushButton;
    m_shutterButton->setFixedSize(76, 76);
    connect(m_shutterButton, &QPushButton::clicked, this, [this]() {
        if (m_mode == PhotoMode)
            takePhoto();
        else
            toggleRecording();
    });

    QPushButton *cameraButton = new QPushButton(QStringLiteral("1x"));
    cameraButton->setFixedSize(72, 72);
    cameraButton->setStyleSheet(pillButtonStyle());
    cameraButton->setEnabled(false);

    bottomLayout->addWidget(m_albumButton, 0, Qt::AlignCenter);
    bottomLayout->addStretch();
    bottomLayout->addWidget(m_shutterButton, 0, Qt::AlignCenter);
    bottomLayout->addStretch();
    bottomLayout->addWidget(cameraButton, 0, Qt::AlignCenter);

    m_statusLabel = new QLabel(m_cameraPage);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet("color: white; background-color: rgba(0, 0, 0, 140);"
                                 "border-radius: 18px; padding: 7px 18px; font-size: 15px;");
    m_statusLabel->hide();

    updateModeUi();
    m_stack->addWidget(m_cameraPage);
}

void CameraWindow::buildAlbumPage()
{
    m_albumPage = new QWidget;
    m_albumPage->setStyleSheet("background-color: #000000;");

    m_albumImage = new QLabel(m_albumPage);
    m_albumImage->setAlignment(Qt::AlignCenter);
    m_albumImage->setStyleSheet("background-color: #000000; color: white; font-size: 24px;");

    m_albumTopBar = new QWidget(m_albumPage);
    m_albumTopBar->setStyleSheet(PanelStyle);
    QHBoxLayout *topLayout = new QHBoxLayout(m_albumTopBar);
    topLayout->setContentsMargins(18, 10, 18, 10);
    m_albumBackButton = new QPushButton(QStringLiteral("BACK"));
    m_albumBackButton->setFixedSize(96, 44);
    m_albumBackButton->setStyleSheet(pillButtonStyle());
    m_albumTitle = new QLabel(QStringLiteral("Album"));
    m_albumTitle->setAlignment(Qt::AlignCenter);
    m_albumTitle->setStyleSheet("color: white; font-size: 20px; font-weight: 800;");
    connect(m_albumBackButton, &QPushButton::clicked, this, &CameraWindow::closeAlbum);
    topLayout->addWidget(m_albumBackButton);
    topLayout->addStretch();
    topLayout->addWidget(m_albumTitle);
    topLayout->addStretch();
    topLayout->addSpacing(96);

    m_albumBottomBar = new QWidget(m_albumPage);
    m_albumBottomBar->setStyleSheet(PanelStyle);
    QHBoxLayout *bottomLayout = new QHBoxLayout(m_albumBottomBar);
    bottomLayout->setContentsMargins(32, 18, 32, 18);
    m_albumPrevButton = new QPushButton(QStringLiteral("PREV"));
    m_albumNextButton = new QPushButton(QStringLiteral("NEXT"));
    m_albumPrevButton->setFixedSize(128, 54);
    m_albumNextButton->setFixedSize(128, 54);
    m_albumPrevButton->setStyleSheet(pillButtonStyle());
    m_albumNextButton->setStyleSheet(pillButtonStyle());
    connect(m_albumPrevButton, &QPushButton::clicked, this, &CameraWindow::previousAlbum);
    connect(m_albumNextButton, &QPushButton::clicked, this, &CameraWindow::nextAlbum);
    bottomLayout->addWidget(m_albumPrevButton);
    bottomLayout->addStretch();
    bottomLayout->addWidget(m_albumNextButton);

    m_stack->addWidget(m_albumPage);
}

void CameraWindow::enterCamera()
{
    m_stack->setCurrentWidget(m_cameraPage);
    layoutCameraPage();

    if (!m_capture) {
        m_capture = new V4L2Capture(this);
        m_capture->configure(m_device, m_requestWidth, m_requestHeight, m_requestFps);
        connect(m_capture, &V4L2Capture::frameReady, this, &CameraWindow::onFrameReady);
        connect(m_capture, &V4L2Capture::cameraError, this, &CameraWindow::onCameraError);
        connect(m_capture, &V4L2Capture::cameraInfo, this, &CameraWindow::onCameraInfo);
        m_capture->start();
    }
}

void CameraWindow::layoutCameraPage()
{
    if (!m_cameraPage || !m_previewLabel)
        return;

    QRect rect = m_cameraPage->rect();
    int bottomHeight = qMax(118, rect.height() / 5);
    m_previewLabel->setGeometry(0, 0, rect.width(), rect.height() - bottomHeight);

    int topWidth = qMin(rect.width() - 28, 760);
    m_topBar->setGeometry((rect.width() - topWidth) / 2, 16, topWidth, 62);

    m_bottomBar->setGeometry(0, rect.height() - bottomHeight, rect.width(), bottomHeight);

    int modeWidth = 230;
    int modeHeight = 46;
    m_modeBar->setGeometry((rect.width() - modeWidth) / 2,
                           rect.height() - bottomHeight - modeHeight - 10,
                           modeWidth, modeHeight);

    int statusWidth = 360;
    m_statusLabel->setGeometry((rect.width() - statusWidth) / 2,
                               m_topBar->geometry().bottom() + 18,
                               statusWidth, 42);

    m_previewLabel->lower();
    m_topBar->raise();
    m_modeBar->raise();
    m_bottomBar->raise();
    m_statusLabel->raise();
}

void CameraWindow::layoutAlbumPage()
{
    if (!m_albumPage)
        return;

    QRect rect = m_albumPage->rect();
    int topHeight = 70;
    int bottomHeight = 92;
    m_albumTopBar->setGeometry(0, 0, rect.width(), topHeight);
    m_albumBottomBar->setGeometry(0, rect.height() - bottomHeight,
                                  rect.width(), bottomHeight);
    m_albumImage->setGeometry(0, topHeight, rect.width(),
                              rect.height() - topHeight - bottomHeight);
}

void CameraWindow::onFrameReady(const QImage &image)
{
    m_currentFrame = image;

    if (m_stack->currentWidget() == m_cameraPage)
        updatePreview();

    if (m_capture)
        m_capture->notifyFrameConsumed();

    if (m_recording) {
        int interval = 1000 / qMax(1, m_recordFps);
        if ((!m_recordFrameTimer.isValid() || m_recordFrameTimer.elapsed() >= interval) &&
            !m_recordFramePending) {
            m_recordFramePending = true;
            QMetaObject::invokeMethod(m_recordWorker, "addFrame", Qt::QueuedConnection,
                                      Q_ARG(QImage, m_currentFrame));
            m_recordFrameTimer.restart();
        }
    }
}

void CameraWindow::onCameraError(const QString &message)
{
    m_previewLabel->setText(QStringLiteral("Camera error\n%1").arg(message));
    m_statusLabel->setText(message);
    m_statusLabel->show();
}

void CameraWindow::onCameraInfo(const QString &message)
{
    m_deviceLabel->setText(message);
}

void CameraWindow::takePhoto()
{
    if (m_currentFrame.isNull()) {
        m_statusLabel->setText(QStringLiteral("No frame yet"));
        m_statusLabel->show();
        return;
    }

    QString path = makeTimestampPath(QStringLiteral("photo"), QStringLiteral("IMG_"),
                                     QStringLiteral(".jpg"));
    if (!m_currentFrame.save(path, "JPG", 95)) {
        path = makeTimestampPath(QStringLiteral("photo"), QStringLiteral("IMG_"),
                                 QStringLiteral(".png"));
        if (!m_currentFrame.save(path, "PNG")) {
            m_statusLabel->setText(QStringLiteral("Photo save failed"));
            m_statusLabel->show();
            return;
        }
    }

    updateThumbnail(m_currentFrame);
    m_statusLabel->setText(QStringLiteral("Saved %1").arg(QFileInfo(path).fileName()));
    m_statusLabel->show();
    QTimer::singleShot(1100, m_statusLabel, &QLabel::hide);
}

void CameraWindow::toggleRecording()
{
    if (m_recording) {
        stopRecording();
        return;
    }

    if (m_currentFrame.isNull()) {
        m_statusLabel->setText(QStringLiteral("No frame yet"));
        m_statusLabel->show();
        return;
    }

    QString path = makeTimestampPath(QStringLiteral("video"), QStringLiteral("VID_"),
                                     QStringLiteral(".avi"));
    bool started = false;
    QMetaObject::invokeMethod(m_recordWorker, "start", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, started),
                              Q_ARG(QString, path),
                              Q_ARG(int, m_currentFrame.width()),
                              Q_ARG(int, m_currentFrame.height()),
                              Q_ARG(int, m_recordFps));
    if (!started) {
        m_statusLabel->setText(QStringLiteral("Cannot create video file"));
        m_statusLabel->show();
        return;
    }

    m_recording = true;
    m_recordFramePending = false;
    m_recordPath = path;
    m_recordTimer.restart();
    m_recordFrameTimer.invalidate();
    m_recordUiTimer.start(200);
    updateRecordUi();
    m_shutterButton->setStyleSheet(
        "QPushButton { background-color: #ff2d55; border: 14px solid white; border-radius: 20px; }"
        "QPushButton:pressed { background-color: #c9183f; }");
}

void CameraWindow::stopRecording()
{
    if (!m_recording)
        return;

    QString fileName = QFileInfo(m_recordPath).fileName();
    m_recording = false;
    m_recordFramePending = false;
    QMetaObject::invokeMethod(m_recordWorker, "stop", Qt::BlockingQueuedConnection);
    m_recordUiTimer.stop();
    m_recordLabel->setText(QStringLiteral("SAVED"));
    m_statusLabel->setText(QStringLiteral("Saved %1").arg(fileName));
    m_statusLabel->show();
    QTimer::singleShot(1400, m_statusLabel, &QLabel::hide);
    updateModeUi();
}

void CameraWindow::switchToPhotoMode()
{
    if (m_recording)
        stopRecording();
    m_mode = PhotoMode;
    updateModeUi();
}

void CameraWindow::switchToVideoMode()
{
    m_mode = VideoMode;
    updateModeUi();
}

void CameraWindow::openAlbum()
{
    if (m_recording)
        stopRecording();
    refreshAlbum();
    m_stack->setCurrentWidget(m_albumPage);
    layoutAlbumPage();
    showAlbumImage();
}

void CameraWindow::closeAlbum()
{
    m_stack->setCurrentWidget(m_cameraPage);
    layoutCameraPage();
    updatePreview();
}

void CameraWindow::nextAlbum()
{
    if (m_albumFiles.isEmpty())
        return;
    m_albumIndex = (m_albumIndex + 1) % m_albumFiles.size();
    showAlbumImage();
}

void CameraWindow::previousAlbum()
{
    if (m_albumFiles.isEmpty())
        return;
    m_albumIndex = (m_albumIndex + m_albumFiles.size() - 1) % m_albumFiles.size();
    showAlbumImage();
}

void CameraWindow::updateRecordUi()
{
    if (!m_recording)
        return;

    m_recordLabel->setText(recordTimeText(m_recordTimer.elapsed()));
    m_recordLabel->setStyleSheet("color: #ff453a; font-size: 15px; font-weight: 900;");
}

void CameraWindow::updatePreview()
{
    if (!m_previewLabel || m_currentFrame.isNull())
        return;
    m_previewLabel->setPixmap(scaledToFill(m_currentFrame, m_previewLabel->size(),
                                           Qt::FastTransformation));
}

void CameraWindow::updateModeUi()
{
    m_photoModeButton->setStyleSheet(textButtonStyle(m_mode == PhotoMode));
    m_videoModeButton->setStyleSheet(textButtonStyle(m_mode == VideoMode));
    m_shutterButton->setStyleSheet(shutterStyle(m_mode == VideoMode));

    if (!m_recording) {
        m_recordLabel->setText(m_mode == VideoMode ? QStringLiteral("VIDEO")
                                                   : QStringLiteral("PHOTO"));
        m_recordLabel->setStyleSheet("color: #b8c0cc; font-size: 15px; font-weight: 700;");
    }
}

void CameraWindow::updateThumbnail(const QImage &image)
{
    QPixmap pix = scaledToFill(image, QSize(86, 62), Qt::SmoothTransformation);
    m_albumButton->setText(QString());
    m_albumButton->setIcon(QIcon(pix));
    m_albumButton->setIconSize(QSize(86, 62));
}

void CameraWindow::refreshAlbum()
{
    ensureDir(QStringLiteral("photo"));
    QDir dir(QStringLiteral("photo"));
    QFileInfoList files = dir.entryInfoList(QStringList()
                                                << "*.jpg" << "*.jpeg"
                                                << "*.png" << "*.bmp",
                                            QDir::Files, QDir::Name);

    m_albumFiles.clear();
    for (const QFileInfo &file : files)
        m_albumFiles.append(file.absoluteFilePath());

    m_albumIndex = m_albumFiles.isEmpty() ? -1 : m_albumFiles.size() - 1;
}

void CameraWindow::showAlbumImage()
{
    if (!m_albumImage || m_stack->currentWidget() != m_albumPage)
        return;

    if (m_albumFiles.isEmpty() || m_albumIndex < 0) {
        m_albumTitle->setText(QStringLiteral("Album"));
        m_albumImage->setText(QStringLiteral("NO PHOTO"));
        m_albumImage->setPixmap(QPixmap());
        return;
    }

    QImage image(m_albumFiles[m_albumIndex]);
    if (image.isNull()) {
        m_albumImage->setText(QStringLiteral("LOAD FAILED"));
        m_albumImage->setPixmap(QPixmap());
        return;
    }

    m_albumTitle->setText(QStringLiteral("%1 / %2")
                              .arg(m_albumIndex + 1)
                              .arg(m_albumFiles.size()));
    m_albumImage->setText(QString());
    m_albumImage->setPixmap(scaledToFit(image, m_albumImage->size()));
}

QString CameraWindow::makeTimestampPath(const QString &dirName,
                                        const QString &prefix,
                                        const QString &suffix) const
{
    ensureDir(dirName);
    QString base = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QDir dir(dirName);
    for (int i = 0; i < 100; ++i) {
        QString name = i == 0
            ? QStringLiteral("%1%2%3").arg(prefix, base, suffix)
            : QStringLiteral("%1%2_%3%4")
                  .arg(prefix, base)
                  .arg(i, 2, 10, QLatin1Char('0'))
                  .arg(suffix);
        QString path = dir.filePath(name);
        if (!QFileInfo::exists(path))
            return path;
    }
    return dir.filePath(QStringLiteral("%1%2_last%3").arg(prefix, base, suffix));
}

bool CameraWindow::ensureDir(const QString &dirName)
{
    QDir dir;
    return dir.exists(dirName) || dir.mkpath(dirName);
}

QPixmap CameraWindow::scaledToFill(const QImage &image, const QSize &size,
                                   Qt::TransformationMode mode)
{
    if (image.isNull() || size.isEmpty())
        return QPixmap();

    QPixmap src = QPixmap::fromImage(image).scaled(size, Qt::KeepAspectRatioByExpanding,
                                                   mode);
    QPixmap out(size);
    out.fill(Qt::black);
    QPainter painter(&out);
    painter.drawPixmap((size.width() - src.width()) / 2,
                       (size.height() - src.height()) / 2,
                       src);
    return out;
}

QPixmap CameraWindow::scaledToFit(const QImage &image, const QSize &size)
{
    if (image.isNull() || size.isEmpty())
        return QPixmap();

    QPixmap src = QPixmap::fromImage(image).scaled(size, Qt::KeepAspectRatio,
                                                   Qt::SmoothTransformation);
    QPixmap out(size);
    out.fill(Qt::black);
    QPainter painter(&out);
    painter.drawPixmap((size.width() - src.width()) / 2,
                       (size.height() - src.height()) / 2,
                       src);
    return out;
}
