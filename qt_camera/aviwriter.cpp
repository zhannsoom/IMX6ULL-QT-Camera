#include "aviwriter.h"

#include <QBuffer>
#include <QFileInfo>

AviWriter::AviWriter() = default;

AviWriter::~AviWriter()
{
    stop();
}

bool AviWriter::start(const QString &filePath, int width, int height, int fps)
{
    stop();

    m_filePath = filePath;
    m_width = width;
    m_height = height;
    m_fps = fps > 0 ? fps : 15;
    m_frames = 0;
    m_index.clear();

    m_file.setFileName(filePath);
    if (!m_file.open(QIODevice::WriteOnly))
        return false;

    return writeHeader();
}

bool AviWriter::addFrame(const QImage &image)
{
    if (!m_file.isOpen() || image.isNull())
        return false;

    QImage frame = image;
    if (frame.width() != m_width || frame.height() != m_height)
        frame = frame.scaled(m_width, m_height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (frame.format() != QImage::Format_RGB888)
        frame = frame.convertToFormat(QImage::Format_RGB888);

    QByteArray jpeg;
    QBuffer buffer(&jpeg);
    buffer.open(QIODevice::WriteOnly);
    if (!frame.save(&buffer, "JPG", 86))
        return false;

    qint64 chunkStart = m_file.pos();
    writeFourcc("00dc");
    writeU32(static_cast<quint32>(jpeg.size()));
    if (m_file.write(jpeg) != jpeg.size())
        return false;
    if (jpeg.size() & 1)
        m_file.putChar('\0');

    IndexEntry entry;
    entry.offset = static_cast<quint32>(chunkStart - m_moviDataStart);
    entry.size = static_cast<quint32>(jpeg.size());
    m_index.append(entry);
    ++m_frames;
    return true;
}

bool AviWriter::stop()
{
    if (!m_file.isOpen())
        return true;

    qint64 idxStart = m_file.pos();
    patchU32(m_moviListSizePos, static_cast<quint32>(idxStart - m_moviListSizePos - 4));

    writeFourcc("idx1");
    writeU32(static_cast<quint32>(m_index.size() * 16));
    for (const IndexEntry &entry : m_index) {
        writeFourcc("00dc");
        writeU32(0x10);
        writeU32(entry.offset);
        writeU32(entry.size);
    }

    qint64 fileSize = m_file.pos();
    patchU32(m_riffSizePos, static_cast<quint32>(fileSize - 8));
    patchU32(m_avihFramesPos, static_cast<quint32>(m_frames));
    patchU32(m_strhFramesPos, static_cast<quint32>(m_frames));

    m_file.close();
    return true;
}

bool AviWriter::isOpen() const
{
    return m_file.isOpen();
}

int AviWriter::frameCount() const
{
    return m_frames;
}

QString AviWriter::filePath() const
{
    return m_filePath;
}

void AviWriter::writeFourcc(const char fourcc[4])
{
    m_file.write(fourcc, 4);
}

void AviWriter::writeU16(quint16 value)
{
    char data[2];
    data[0] = static_cast<char>(value & 0xFF);
    data[1] = static_cast<char>((value >> 8) & 0xFF);
    m_file.write(data, 2);
}

void AviWriter::writeU32(quint32 value)
{
    char data[4];
    data[0] = static_cast<char>(value & 0xFF);
    data[1] = static_cast<char>((value >> 8) & 0xFF);
    data[2] = static_cast<char>((value >> 16) & 0xFF);
    data[3] = static_cast<char>((value >> 24) & 0xFF);
    m_file.write(data, 4);
}

void AviWriter::patchU32(qint64 position, quint32 value)
{
    qint64 current = m_file.pos();
    m_file.seek(position);
    writeU32(value);
    m_file.seek(current);
}

bool AviWriter::writeHeader()
{
    writeFourcc("RIFF");
    m_riffSizePos = m_file.pos();
    writeU32(0);
    writeFourcc("AVI ");

    writeFourcc("LIST");
    qint64 hdrlSizePos = m_file.pos();
    writeU32(0);
    writeFourcc("hdrl");

    writeFourcc("avih");
    writeU32(56);
    writeU32(static_cast<quint32>(1000000 / m_fps));
    writeU32(static_cast<quint32>(m_width * m_height * 3 * m_fps));
    writeU32(0);
    writeU32(0x10);
    m_avihFramesPos = m_file.pos();
    writeU32(0);
    writeU32(0);
    writeU32(1);
    writeU32(static_cast<quint32>(m_width * m_height * 3));
    writeU32(static_cast<quint32>(m_width));
    writeU32(static_cast<quint32>(m_height));
    writeU32(0);
    writeU32(0);
    writeU32(0);
    writeU32(0);

    writeFourcc("LIST");
    qint64 strlSizePos = m_file.pos();
    writeU32(0);
    writeFourcc("strl");

    writeFourcc("strh");
    writeU32(56);
    writeFourcc("vids");
    writeFourcc("MJPG");
    writeU32(0);
    writeU32(0);
    writeU32(0);
    writeU32(1);
    writeU32(static_cast<quint32>(m_fps));
    writeU32(0);
    m_strhFramesPos = m_file.pos();
    writeU32(0);
    writeU32(static_cast<quint32>(m_width * m_height * 3));
    writeU32(0xFFFFFFFF);
    writeU32(0);
    writeU16(0);
    writeU16(0);
    writeU16(static_cast<quint16>(m_width));
    writeU16(static_cast<quint16>(m_height));

    writeFourcc("strf");
    writeU32(40);
    writeU32(40);
    writeU32(static_cast<quint32>(m_width));
    writeU32(static_cast<quint32>(m_height));
    writeU16(1);
    writeU16(24);
    writeFourcc("MJPG");
    writeU32(static_cast<quint32>(m_width * m_height * 3));
    writeU32(0);
    writeU32(0);
    writeU32(0);
    writeU32(0);

    qint64 afterStrl = m_file.pos();
    patchU32(strlSizePos, static_cast<quint32>(afterStrl - strlSizePos - 4));
    qint64 afterHdrl = m_file.pos();
    patchU32(hdrlSizePos, static_cast<quint32>(afterHdrl - hdrlSizePos - 4));

    writeFourcc("LIST");
    m_moviListSizePos = m_file.pos();
    writeU32(0);
    writeFourcc("movi");
    m_moviDataStart = m_file.pos();

    return m_file.error() == QFile::NoError;
}
