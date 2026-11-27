/***************************************************************************
    File                 : ZipReader.cpp
    Project              : ToriGraph / SciDAVis
    Description          : Self-contained read-only ZIP32 / ZIP64 archive reader
                           (moved out of ImportOPJU.cpp so that the .xlsx importer
                           can share it)
 ***************************************************************************/

#include "ZipReader.h"

#include <zlib.h>

#include <QFile>
#include <QTextCodec>

#include <algorithm>
#include <cstring>

namespace torizip {

namespace {

inline bool readU16LE(const QByteArray &buf, qint64 pos, quint16 &out)
{
    if (pos < 0 || pos + 2 > buf.size())
        return false;
    const auto *p = reinterpret_cast<const unsigned char *>(buf.constData() + pos);
    out = static_cast<quint16>(p[0]) | (static_cast<quint16>(p[1]) << 8);
    return true;
}

inline bool readU32LE(const QByteArray &buf, qint64 pos, quint32 &out)
{
    if (pos < 0 || pos + 4 > buf.size())
        return false;
    const auto *p = reinterpret_cast<const unsigned char *>(buf.constData() + pos);
    out = static_cast<quint32>(p[0]) | (static_cast<quint32>(p[1]) << 8)
            | (static_cast<quint32>(p[2]) << 16) | (static_cast<quint32>(p[3]) << 24);
    return true;
}

inline bool readU64LE(const QByteArray &buf, qint64 pos, quint64 &out)
{
    quint32 lo = 0, hi = 0;
    if (!readU32LE(buf, pos, lo) || !readU32LE(buf, pos + 4, hi))
        return false;
    out = static_cast<quint64>(lo) | (static_cast<quint64>(hi) << 32);
    return true;
}

// Entry names without the UTF-8 flag: ASCII, then UTF-8, then Shift-JIS (Japanese Windows
// archivers), then the local 8-bit codec.
QString decodeZipName(const QByteArray &bytes)
{
    bool pureAscii = true;
    for (char ch : bytes) {
        if (static_cast<unsigned char>(ch) >= 0x80) {
            pureAscii = false;
            break;
        }
    }
    if (pureAscii)
        return QString::fromLatin1(bytes);
    if (QTextCodec *utf8 = QTextCodec::codecForName("UTF-8")) {
        QTextCodec::ConverterState state;
        const QString s = utf8->toUnicode(bytes.constData(), bytes.size(), &state);
        if (state.invalidChars == 0)
            return s;
    }
    if (QTextCodec *sjis = QTextCodec::codecForName("Shift-JIS")) {
        QTextCodec::ConverterState state;
        const QString s = sjis->toUnicode(bytes.constData(), bytes.size(), &state);
        if (state.invalidChars == 0)
            return s;
    }
    return QString::fromLocal8Bit(bytes);
}

} // namespace

// ---------------------------------------------------------------------------
// Zlib inflate helper (raw deflate, zlib-wrapped, gzip-wrapped)
// ---------------------------------------------------------------------------
bool inflateBuffer(const char *src, quint64 srcLen, int windowBits, quint64 maxOutBytes,
                   QByteArray &out, QString *outErr)
{
    out.clear();
    z_stream zs {};
    if (inflateInit2(&zs, windowBits) != Z_OK) {
        if (outErr)
            *outErr = QStringLiteral("zlib inflateInit2 failed.");
        return false;
    }

    struct InflateGuard
    {
        z_stream *s;
        ~InflateGuard() { inflateEnd(s); }
    } guard { &zs };

    zs.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(src));
    quint64 remainingIn = srcLen;
    char chunk[32768];

    int ret = Z_OK;
    while (ret != Z_STREAM_END) {
        if (zs.avail_in == 0 && remainingIn > 0) {
            const uInt feed = static_cast<uInt>(
                    std::min<quint64>(remainingIn, static_cast<quint64>(1U << 24)));
            zs.avail_in = feed;
            remainingIn -= feed;
        }

        zs.next_out = reinterpret_cast<Bytef *>(chunk);
        zs.avail_out = static_cast<uInt>(sizeof(chunk));

        ret = inflate(&zs, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            if (outErr)
                *outErr = QStringLiteral("zlib inflate failed (code %1).").arg(ret);
            return false;
        }

        const qint64 produced = static_cast<qint64>(sizeof(chunk) - zs.avail_out);
        if (produced > 0) {
            if (static_cast<quint64>(out.size()) + static_cast<quint64>(produced) > maxOutBytes) {
                if (outErr)
                    *outErr = QStringLiteral("Decompressed stream exceeds maximum allowed size.");
                return false;
            }
            out.append(chunk, static_cast<int>(produced));
        }
        if (ret == Z_OK && produced == 0 && zs.avail_in == 0 && remainingIn == 0) {
            // Truncated stream
            if (outErr)
                *outErr = QStringLiteral("Truncated compressed stream.");
            return false;
        }
    }
    return true;
}

QString normalizeZipPath(QString path)
{
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (path.startsWith(QLatin1String("./")))
        path.remove(0, 2);
    while (path.startsWith(QLatin1Char('/')))
        path.remove(0, 1);
    return path;
}

// ---------------------------------------------------------------------------
// ZipArchive
// ---------------------------------------------------------------------------
bool ZipArchive::open(const QString &fileName, QString *outErr)
{
    QFile f(fileName);
    if (!f.exists()) {
        if (outErr)
            *outErr = QStringLiteral("File does not exist: %1").arg(fileName);
        return false;
    }
    if (f.size() <= 0) {
        if (outErr)
            *outErr = QStringLiteral("File is empty: %1").arg(fileName);
        return false;
    }
    if (f.size() > kMaxArchiveBytes) {
        if (outErr)
            *outErr = QStringLiteral("File exceeds maximum supported size (512 MiB).");
        return false;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        if (outErr)
            *outErr = QStringLiteral("Failed to open file '%1': %2").arg(fileName, f.errorString());
        return false;
    }
    d_data = f.readAll();
    f.close();
    return parseCentralDirectory(outErr);
}

bool ZipArchive::openData(const QByteArray &data, QString *outErr)
{
    if (data.size() > kMaxArchiveBytes) {
        if (outErr)
            *outErr = QStringLiteral("Archive exceeds maximum supported size (512 MiB).");
        return false;
    }
    d_data = data;
    return parseCentralDirectory(outErr);
}

int ZipArchive::findExactEntry(const QString &rawPath) const
{
    const QString norm = normalizeZipPath(rawPath);
    if (norm.isEmpty())
        return -1;
    auto it = d_lookupLower.constFind(norm.toLower());
    return (it != d_lookupLower.constEnd()) ? it.value() : -1;
}

int ZipArchive::findEntry(const QString &rawPath, const QString &baseDir) const
{
    const QString norm = normalizeZipPath(rawPath);
    if (norm.isEmpty())
        return -1;
    auto it = d_lookupLower.constFind(norm.toLower());
    if (it != d_lookupLower.constEnd())
        return it.value();

    if (!baseDir.isEmpty()) {
        const QString combined = normalizeZipPath(baseDir + QLatin1Char('/') + norm);
        it = d_lookupLower.constFind(combined.toLower());
        if (it != d_lookupLower.constEnd())
            return it.value();
    }

    // Suffix fallback for relative data references (e.g. "Book1_A.dat" inside "Data/Book1_A.dat")
    const QString suffix = QLatin1Char('/') + norm.toLower();
    for (int i = 0; i < d_entries.size(); ++i) {
        const QString lowerName = d_entries[i].name.toLower();
        if (lowerName == norm.toLower() || lowerName.endsWith(suffix))
            return i;
    }
    return -1;
}

bool ZipArchive::extract(int index, QByteArray &out, QString *outErr) const
{
    if (index < 0 || index >= d_entries.size()) {
        if (outErr)
            *outErr = QStringLiteral("ZIP entry index out of range.");
        return false;
    }
    const ZipEntry &e = d_entries[index];
    if (e.isEncrypted()) {
        if (outErr)
            *outErr = QStringLiteral("ZIP entry '%1' is encrypted.").arg(e.name);
        return false;
    }
    if (e.localHeaderOffset > static_cast<quint64>(d_data.size())
        || d_data.size() - static_cast<qint64>(e.localHeaderOffset) < 30) {
        if (outErr)
            *outErr = QStringLiteral("Corrupt ZIP local header offset for '%1'.").arg(e.name);
        return false;
    }

    const qint64 lhPos = static_cast<qint64>(e.localHeaderOffset);
    quint32 sig = 0;
    quint16 lhNameLen = 0, lhExtraLen = 0;
    if (!readU32LE(d_data, lhPos, sig) || sig != 0x04034B50U
        || !readU16LE(d_data, lhPos + 26, lhNameLen)
        || !readU16LE(d_data, lhPos + 28, lhExtraLen)) {
        if (outErr)
            *outErr = QStringLiteral("Invalid ZIP local file header for '%1'.").arg(e.name);
        return false;
    }

    const quint64 dataOffset = e.localHeaderOffset + 30ULL + lhNameLen + lhExtraLen;
    if (dataOffset > static_cast<quint64>(d_data.size())
        || e.compSize > static_cast<quint64>(d_data.size()) - dataOffset) {
        if (outErr)
            *outErr = QStringLiteral("Corrupt ZIP compressed data bounds for '%1'.").arg(e.name);
        return false;
    }
    if (e.uncompSize > kMaxEntryUncompressedBytes) {
        if (outErr)
            *outErr = QStringLiteral("ZIP entry '%1' exceeds maximum allowed size.").arg(e.name);
        return false;
    }

    const char *compPtr = d_data.constData() + static_cast<qint64>(dataOffset);
    if (e.method == 0) {
        if (e.compSize > kMaxEntryUncompressedBytes) {
            if (outErr)
                *outErr = QStringLiteral("ZIP entry '%1' exceeds maximum allowed size.").arg(e.name);
            return false;
        }
        out = QByteArray(compPtr, static_cast<int>(e.compSize));
    } else if (e.method == 8) {
        const quint64 entryMax = std::min<quint64>(e.uncompSize, kMaxEntryUncompressedBytes);
        if (!inflateBuffer(compPtr, e.compSize, -MAX_WBITS, entryMax, out, outErr)) {
            if (outErr)
                *outErr = QStringLiteral("Failed to deflate ZIP entry '%1': %2")
                                  .arg(e.name, *outErr);
            return false;
        }
    } else {
        if (outErr)
            *outErr = QStringLiteral("Unsupported ZIP compression method %1 for '%2'.")
                              .arg(e.method)
                              .arg(e.name);
        return false;
    }

    if (static_cast<quint64>(out.size()) != e.uncompSize) {
        out.clear();
        if (outErr)
            *outErr = QStringLiteral("Uncompressed size mismatch in ZIP entry '%1'.").arg(e.name);
        return false;
    }

    const uLong computed = out.isEmpty()
            ? 0UL
            : ::crc32(0L, reinterpret_cast<const Bytef *>(out.constData()),
                      static_cast<uInt>(out.size()));
    if (static_cast<quint32>(computed) != e.crc32) {
        out.clear();
        if (outErr)
            *outErr = QStringLiteral("CRC32 mismatch in ZIP entry '%1'.").arg(e.name);
        return false;
    }
    return true;
}

bool ZipArchive::parseCentralDirectory(QString *outErr)
{
    d_entries.clear();
    d_lookupLower.clear();

    if (d_data.size() < 22) {
        if (outErr)
            *outErr = QStringLiteral("File is too small to be a ZIP archive.");
        return false;
    }

    // Locate End of Central Directory (EOCD: 0x06054B50) scanning backwards
    const qint64 minPos = std::max<qint64>(0, d_data.size() - 22 - 65535);
    qint64 eocdPos = -1;
    for (qint64 p = d_data.size() - 22; p >= minPos; --p) {
        quint32 sig = 0;
        if (readU32LE(d_data, p, sig) && sig == 0x06054B50U) {
            quint16 commentLen = 0;
            if (readU16LE(d_data, p + 20, commentLen) && p + 22 + commentLen == d_data.size()) {
                eocdPos = p;
                break;
            }
            if (eocdPos < 0)
                eocdPos = p; // fallback if trailing bytes exist
        }
    }
    if (eocdPos < 0) {
        if (outErr)
            *outErr = QStringLiteral("Not a valid ZIP archive (EOCD record not found).");
        return false;
    }

    quint16 totalEntries16 = 0;
    quint32 cdSize32 = 0;
    quint32 cdOffset32 = 0;
    if (!readU16LE(d_data, eocdPos + 10, totalEntries16)
        || !readU32LE(d_data, eocdPos + 12, cdSize32)
        || !readU32LE(d_data, eocdPos + 16, cdOffset32)) {
        if (outErr)
            *outErr = QStringLiteral("Corrupt ZIP EOCD header.");
        return false;
    }

    quint64 totalEntries = totalEntries16;
    quint64 cdSize = cdSize32;
    quint64 cdOffset = cdOffset32;

    // Check for ZIP64 EOCD Locator immediately before EOCD (20 bytes)
    if (eocdPos >= 20) {
        quint32 locSig = 0;
        if (readU32LE(d_data, eocdPos - 20, locSig) && locSig == 0x07064B50U) {
            quint64 zip64EocdOff = 0;
            if (readU64LE(d_data, eocdPos - 12, zip64EocdOff)
                && zip64EocdOff + 56ULL <= static_cast<quint64>(d_data.size())) {
                quint32 z64Sig = 0;
                if (readU32LE(d_data, static_cast<qint64>(zip64EocdOff), z64Sig)
                    && z64Sig == 0x06064B50U) {
                    quint64 z64Entries = 0, z64CdSize = 0, z64CdOffset = 0;
                    if (readU64LE(d_data, static_cast<qint64>(zip64EocdOff + 32), z64Entries)
                        && readU64LE(d_data, static_cast<qint64>(zip64EocdOff + 40), z64CdSize)
                        && readU64LE(d_data, static_cast<qint64>(zip64EocdOff + 48),
                                     z64CdOffset)) {
                        if (totalEntries16 == 0xFFFFU)
                            totalEntries = z64Entries;
                        if (cdSize32 == 0xFFFFFFFFU)
                            cdSize = z64CdSize;
                        if (cdOffset32 == 0xFFFFFFFFU)
                            cdOffset = z64CdOffset;
                    }
                }
            }
        }
    }

    if (totalEntries > static_cast<quint64>(kMaxZipEntries)) {
        if (outErr)
            *outErr = QStringLiteral("ZIP archive has too many entries (%1).").arg(totalEntries);
        return false;
    }
    if (cdOffset > static_cast<quint64>(d_data.size())
        || cdSize > static_cast<quint64>(d_data.size()) - cdOffset) {
        if (outErr)
            *outErr = QStringLiteral("Corrupt ZIP Central Directory bounds.");
        return false;
    }

    qint64 pos = static_cast<qint64>(cdOffset);
    const qint64 cdEnd = static_cast<qint64>(cdOffset + cdSize);
    d_entries.reserve(static_cast<int>(totalEntries));

    for (quint64 i = 0; i < totalEntries; ++i) {
        if (pos + 46 > cdEnd) {
            if (outErr)
                *outErr = QStringLiteral("Truncated ZIP Central Directory entry.");
            return false;
        }
        quint32 sig = 0;
        quint16 flags = 0, method = 0, nameLen = 0, extraLen = 0, commentLen = 0;
        quint32 crc = 0, comp32 = 0, uncomp32 = 0, lhOff32 = 0;
        if (!readU32LE(d_data, pos, sig) || sig != 0x02014B50U
            || !readU16LE(d_data, pos + 8, flags) || !readU16LE(d_data, pos + 10, method)
            || !readU32LE(d_data, pos + 16, crc) || !readU32LE(d_data, pos + 20, comp32)
            || !readU32LE(d_data, pos + 24, uncomp32) || !readU16LE(d_data, pos + 28, nameLen)
            || !readU16LE(d_data, pos + 30, extraLen) || !readU16LE(d_data, pos + 32, commentLen)
            || !readU32LE(d_data, pos + 42, lhOff32)) {
            if (outErr)
                *outErr = QStringLiteral("Corrupt ZIP Central Directory header.");
            return false;
        }

        const qint64 entryTotal = 46LL + nameLen + extraLen + commentLen;
        if (pos + entryTotal > cdEnd) {
            if (outErr)
                *outErr = QStringLiteral("ZIP Central Directory entry extends past directory end.");
            return false;
        }

        const QByteArray rawName(d_data.constData() + pos + 46, nameLen);
        QString decodedName =
                (flags & 0x0800U) ? QString::fromUtf8(rawName) : decodeZipName(rawName);
        decodedName = normalizeZipPath(decodedName);

        ZipEntry entry;
        entry.name = decodedName;
        entry.flags = flags;
        entry.method = method;
        entry.crc32 = crc;
        entry.compSize = comp32;
        entry.uncompSize = uncomp32;
        entry.localHeaderOffset = lhOff32;

        // Parse ZIP64 Extra Field (header ID 0x0001) if any 32-bit field is sentinel
        if (uncomp32 == 0xFFFFFFFFU || comp32 == 0xFFFFFFFFU || lhOff32 == 0xFFFFFFFFU) {
            qint64 exPos = pos + 46 + nameLen;
            const qint64 exEnd = exPos + extraLen;
            while (exPos + 4 <= exEnd) {
                quint16 headerId = 0, fieldSize = 0;
                if (!readU16LE(d_data, exPos, headerId)
                    || !readU16LE(d_data, exPos + 2, fieldSize)) {
                    break;
                }
                exPos += 4;
                if (exPos + fieldSize > exEnd)
                    break;
                if (headerId == 0x0001U) {
                    qint64 zPos = exPos;
                    const qint64 zEnd = exPos + fieldSize;
                    if (uncomp32 == 0xFFFFFFFFU && zPos + 8 <= zEnd) {
                        readU64LE(d_data, zPos, entry.uncompSize);
                        zPos += 8;
                    }
                    if (comp32 == 0xFFFFFFFFU && zPos + 8 <= zEnd) {
                        readU64LE(d_data, zPos, entry.compSize);
                        zPos += 8;
                    }
                    if (lhOff32 == 0xFFFFFFFFU && zPos + 8 <= zEnd) {
                        readU64LE(d_data, zPos, entry.localHeaderOffset);
                        zPos += 8;
                    }
                    break;
                }
                exPos += fieldSize;
            }
        }

        if (!entry.name.isEmpty() && !entry.name.endsWith(QLatin1Char('/'))) {
            const int idx = d_entries.size();
            d_entries.append(entry);
            d_lookupLower.insert(entry.name.toLower(), idx);
        }

        pos += entryTotal;
    }
    return true;
}

} // namespace torizip
