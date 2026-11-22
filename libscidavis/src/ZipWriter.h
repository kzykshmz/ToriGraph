/***************************************************************************
    File                 : ZipWriter.h
    Project              : ToriGraph / SciDAVis
    Description          : Small write-only ZIP32 archive builder (stored or raw
                           deflate via zlib). The counterpart of ZipReader, used
                           to write Office Open XML packages (.pptx) without
                           Qt's private QZipWriter.
 ***************************************************************************/

#ifndef TORIGRAPH_ZIPWRITER_H
#define TORIGRAPH_ZIPWRITER_H

#include <QByteArray>
#include <QDateTime>
#include <QSet>
#include <QString>
#include <QVector>

namespace torizip {

class ZipWriter
{
public:
    enum class Compression {
        Store, //!< method 0
        Deflate //!< method 8; falls back to Store when deflate does not make the entry smaller
    };

    //! Time stamp written into every entry (local time, 2-second resolution). Defaults to the
    //! time the writer was created.
    void setTimestamp(const QDateTime &when);

    //! Appends one entry. `name` uses forward slashes and no leading '/'. Fails (with a
    //! message in *outErr) for an empty or duplicate name, after finish(), or when the archive
    //! would need ZIP64 (more than 65535 entries or 4 GiB).
    bool addFile(const QString &name, const QByteArray &data,
                 Compression compression = Compression::Deflate, QString *outErr = nullptr);

    //! Writes the central directory and returns the complete archive. Further addFile() calls
    //! fail; calling finish() again returns the same bytes.
    QByteArray finish();

    int entryCount() const { return d_entries.size(); }

    //! CRC-32 (ISO 3309 / ZIP), via zlib.
    static quint32 crc32(const QByteArray &data);

private:
    struct Entry
    {
        QByteArray nameUtf8;
        quint16 flags = 0;
        quint16 method = 0;
        quint32 crc = 0;
        quint32 compSize = 0;
        quint32 uncompSize = 0;
        quint32 localHeaderOffset = 0;
    };

    void ensureTimestamp();

    QByteArray d_out;
    QVector<Entry> d_entries;
    QSet<QString> d_namesLower;
    quint16 d_dosTime = 0;
    quint16 d_dosDate = 0;
    bool d_hasTimestamp = false;
    bool d_finished = false;
};

} // namespace torizip

#endif // TORIGRAPH_ZIPWRITER_H
