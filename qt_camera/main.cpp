#include "camerawindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("GDUT Qt Camera");
    QCoreApplication::setApplicationVersion("1.0.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Embedded Linux Qt camera based on V4L2");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption deviceOption({"d", "device"},
                                    "V4L2 camera device, default: CAM_DEV or /dev/video1.",
                                    "device");
    QCommandLineOption widthOption({"w", "width"},
                                   "Requested camera width, default: 800.",
                                   "width", "800");
    QCommandLineOption heightOption("height",
                                    "Requested camera height, default: 480.",
                                    "height", "480");
    QCommandLineOption fpsOption({"f", "fps"},
                                 "Requested camera fps, default: 30.",
                                 "fps", "30");
    QCommandLineOption windowedOption("windowed",
                                      "Run in a window instead of fullscreen.");

    parser.addOption(deviceOption);
    parser.addOption(widthOption);
    parser.addOption(heightOption);
    parser.addOption(fpsOption);
    parser.addOption(windowedOption);
    parser.process(app);

    QString device = parser.value(deviceOption);
    if (device.isEmpty()) {
        QByteArray envDevice = qgetenv("CAM_DEV");
        device = envDevice.isEmpty() ? QStringLiteral("/dev/video1")
                                     : QString::fromLocal8Bit(envDevice);
    }

    bool ok = false;
    int width = parser.value(widthOption).toInt(&ok);
    if (!ok || width <= 0)
        width = 800;
    int height = parser.value(heightOption).toInt(&ok);
    if (!ok || height <= 0)
        height = 480;
    int fps = parser.value(fpsOption).toInt(&ok);
    if (!ok || fps <= 0)
        fps = 30;

    CameraWindow window(device, width, height, fps);
    if (parser.isSet(windowedOption))
        window.show();
    else
        window.showFullScreen();

    return app.exec();
}
