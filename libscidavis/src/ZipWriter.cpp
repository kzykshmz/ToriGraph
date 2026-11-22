/***************************************************************************
    File                 : ZipWriter.cpp
    Project              : ToriGraph / SciDAVis
    Description          : Small write-only ZIP32 archive builder (see ZipWriter.h)
 ***************************************************************************/

#include "ZipWriter.h"

#include <zlib.h>

#include <QObject>

#include <limits>

namespace torizip {

namespace {

void putU16(QByteArray &out, quint16 v)
{
    out.append(static_cast<char>(v & 0xFF));
    out.append(static_cast<char>((v >> 8) & 0xFF));
}

void putU32(QByteArray &out, quint32 v)
{
    out.append(static_cast<char>(v & 0xFF));
    out.append(static_cast<char>((v >> 8) & 0xFF));
    out.append(static_cast<char>((v >> 16) & 0xFF));
    out.append(static_cast<char>((v >> 24) & 0xFF));
}

// Raw deflate (no zlib header), as stored in ZIP entries.
bool deflateRaw(const QByteArray &in, QByteArray &out)
{
    z_stream zs{};
    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY)
        != Z_OK)
        return false;
    const uLong bound = deflateBound(&zs, static_cast<uLong>(in.size()));
    out.resize(static_cast<int>(bound));
    zs.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(in.constData()));
    zs.avail_in = static_cast<uInt>(in.size());
    zs.next_out = reinterpret_cast<Bytef *>(out.data());
    zs.avail_out = static_cast<uInt>(out.size());
    const int rc = deflate(&zs, Z_FINISH);
    const uLong produced = zs.total_out;
    deflateEnd(&zs);
    if (rc != Z_STREAM_END)
        return false;
    out.resize(static_cast<int>(produced));
    return true;
}

bool isAscii(const QString &s)
{
    for (const QChar c : s) {
        if (c.unicode() > 0x7F)
            return false;
    }
    return true;
}

} // namespace

quint32 ZipWriter::crc32(const QByteArray &data)
{
    uLong c = ::crc32(0L, Z_NULL, 0);
    c = ::crc32(c, reinterpret_cast<const Bytef *>(data.constData()),
                static_cast<uInt>(data.size()));
    return static_cast<quint32>(c);
}

void ZipWriter::setTimestamp(const QDateTime &when)
{
    const QDateTime t = when.isValid() ? when.toLocalTime() : QDateTime::currentDateTime();
    const QDate d = t.date();
    const QTime tm = t.time();
    const int year = qBound(1980, d.year(), 2107);
    d_dosDate = static_cast<quint16>(((year - 1980) << 9) | (d.month() << 5) | d.day());
    d_dosTime = static_cast<quint16>((tm.hour() << 11) | (tm.minute() << 5) | (tm.second() / 2));
    d_hasTimestamp = true;
}

void ZipWriter::ensureTimestamp()
{
    if (!d_hasTimestamp)
        setTimestamp(QDateTime::currentDateTime());
}

bool ZipWriter::addFile(const QString &name, const QByteArray &data, Compression compression,
                        QString *outErr)
{
    auto fail = [outErr](const QString &msg) {
        if (outErr)
            *outErr = msg;
        return false;
    };
    if (d_finished)
        return fail(QObject::tr("The ZIP archive is already finished."));
    QString n = name;
    n.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (n.startsWith(QLatin1Char('/')))
        n.remove(0, 1);
    if (n.isEmpty())
        return fail(QObject::tr("A ZIP entry needs a name."));
    const QString key = n.toLower();
    if (d_namesLower.contains(key))
        return fail(QObject::tr("Duplicate ZIP entry: %1").arg(n));
    if (d_entries.size() >= 65535)
        return fail(QObject::tr("Too many ZIP entries."));
    ensureTimestamp();

    Entry e;
    e.nameUtf8 = n.toUtf8();
    if (!isAscii(n))
        e.flags |= 0x0800; // language encoding flag: name is UTF-8
    e.crc = crc32(data);
    e.uncompSize = static_cast<quint32>(data.size());

    QByteArray payload;
    e.method = 0;
    if (compression == Compression::Deflate && !data.isEmpty()) {
        QByteArray packed;
        if (deflateRaw(data, packed) && packed.size() < data.size()) {
            payload = packed;
            e.method = 8;
        }
    }
    if (e.method == 0)
        payload = data;
    e.compSize = static_cast<quint32>(payload.size());

    const qint64 needed = qint64(d_out.size()) + 30 + e.nameUtf8.size() + payload.size();
    if (needed >= qint64(std::numeric_limits<int>::max()) - 1024 * 1024)
        return fail(QObject::tr("The ZIP archive would be too large."));

    e.localHeaderOffset = static_cast<quint32>(d_out.size());
    putU32(d_out, 0x04034b50U);
    putU16(d_out, e.method == 8 ? 20 : 10); // version needed to extract
    putU16(d_out, e.flags);
    putU16(d_out, e.method);
    putU16(d_out, d_dosTime);
    putU16(d_out, d_dosDate);
    putU32(d_out, e.crc);
    putU32(d_out, e.compSize);
    putU32(d_out, e.uncompSize);
    putU16(d_out, static_cast<quint16>(e.nameUtf8.size()));
    putU16(d_out, 0); // extra field length
    d_out.append(e.nameUtf8);
    d_out.append(payload);

    d_entries.append(e);
    d_namesLower.insert(key);
    return true;
}

QByteArray ZipWriter::finish()
{
    if (d_finished)
        return d_out;
    ensureTimestamp();
    const quint32 cdOffset = static_cast<quint32>(d_out.size());
    for (const Entry &e : d_entries) {
        putU32(d_out, 0x02014b50U);
        putU16(d_out, 20); // version made by (MS-DOS, 2.0)
        putU16(d_out, e.method == 8 ? 20 : 10);
        putU16(d_out, e.flags);
        putU16(d_out, e.method);
        putU16(d_out, d_dosTime);
        putU16(d_out, d_dosDate);
        putU32(d_out, e.crc);
        putU32(d_out, e.compSize);
        putU32(d_out, e.uncompSize);
        putU16(d_out, static_cast<quint16>(e.nameUtf8.size()));
        putU16(d_out, 0); // extra
        putU16(d_out, 0); // comment
        putU16(d_out, 0); // disk number start
        putU16(d_out, 0); // internal attributes
        putU32(d_out, 0); // external attributes
        putU32(d_out, e.localHeaderOffset);
        d_out.append(e.nameUtf8);
    }
    const quint32 cdSize = static_cast<quint32>(d_out.size()) - cdOffset;
    putU32(d_out, 0x06054b50U);
    putU16(d_out, 0); // number of this disk
    putU16(d_out, 0); // disk with the central directory
    putU16(d_out, static_cast<quint16>(d_entries.size()));
    putU16(d_out, static_cast<quint16>(d_entries.size()));
    putU32(d_out, cdSize);
    putU32(d_out, cdOffset);
    putU16(d_out, 0); // comment length
    d_finished = true;
    return d_out;
}

} // namespace torizip
