/***************************************************************************
    File                 : ZipReader.h
    Project              : ToriGraph / SciDAVis
    Description          : Self-contained read-only ZIP32 / ZIP64 archive reader
                           (stored + raw deflate via zlib). Shared by the Origin
                           .opju importer and the Excel .xlsx importer.
 ***************************************************************************/

#ifndef TORIGRAPH_ZIPREADER_H
#define TORIGRAPH_ZIPREADER_H

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVector>

namespace torizip {

constexpr qint64 kMaxArchiveBytes = 512LL * 1024LL * 1024LL; // 512 MiB
constexpr quint64 kMaxEntryUncompressedBytes = 256ULL * 1024ULL * 1024ULL; // 256 MiB
constexpr int kMaxZipEntries = 65535;

//! Inflates `srcLen` bytes at `src` with zlib. `windowBits` follows inflateInit2():
//! -MAX_WBITS for raw deflate, MAX_WBITS + 32 for zlib/gzip auto-detection.
//! Fails (with a message in *outErr) when the output would exceed `maxOutBytes`.
bool inflateBuffer(const char *src, quint64 srcLen, int windowBits, quint64 maxOutBytes,
                   QByteArray &out, QString *outErr);

//! Forward slashes, no leading "./" or "/".
QString normalizeZipPath(QString path);

struct ZipEntry
{
    QString name; // normalized forward-slash path
    quint16 flags = 0;
    quint16 method = 0;
    quint32 crc32 = 0;
    quint64 compSize = 0;
    quint64 uncompSize = 0;
    quint64 localHeaderOffset = 0;

    bool isEncrypted() const { return (flags & 0x0001U) != 0; }
};

class ZipArchive
{
public:
    //! Reads the whole file (at most kMaxArchiveBytes) and parses its central directory.
    bool open(const QString &fileName, QString *outErr);
    //! Parses an archive that is already in memory.
    bool openData(const QByteArray &data, QString *outErr);

    const QVector<ZipEntry> &entries() const { return d_entries; }

    //! Case-insensitive lookup; falls back to `baseDir`/rawPath and to a path-suffix match.
    int findEntry(const QString &rawPath, const QString &baseDir = QString()) const;
    //! Exact (case-insensitive) lookup only, no suffix fallback.
    int findExactEntry(const QString &rawPath) const;

    bool extract(int index, QByteArray &out, QString *outErr) const;

private:
    bool parseCentralDirectory(QString *outErr);

    QByteArray d_data;
    QVector<ZipEntry> d_entries;
    QHash<QString, int> d_lookupLower;
};

} // namespace torizip

#endif // TORIGRAPH_ZIPREADER_H
