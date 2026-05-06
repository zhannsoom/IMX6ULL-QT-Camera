#ifndef AVIWRITER_H
#define AVIWRITER_H

#include <QFile>
#include <QImage>
#include <QVector>

class AviWriter
{
public:
    AviWriter();
    ~AviWriter();

    bool start(const QString &filePath, int width, int height, int fps);
    bool addFrame(const QImage &image);
    bool stop();
    bool isOpen() const;
    int frameCount() const;
    QString filePath() const;

private:
    struct IndexEntry {
        quint32 offset = 0;
        quint32 size = 0;
    };

    void writeFourcc(const char fourcc[4]);
    void writeU16(quint16 value);
    void writeU32(quint32 value);
    void patchU32(qint64 position, quint32 value);
    bool writeHeader();

    QFile m_file;
    QString m_filePath;
    int m_width = 0;
    int m_height = 0;
    int m_fps = 15;
    int m_frames = 0;

    qint64 m_riffSizePos = 0;
    qint64 m_avihFramesPos = 0;
    qint64 m_strhFramesPos = 0;
    qint64 m_moviListSizePos = 0;
    qint64 m_moviDataStart = 0;
    QVector<IndexEntry> m_index;
};

#endif
