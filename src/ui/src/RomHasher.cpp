#include "RomHasher.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

static constexpr qint64 kChunkSize = 128 * 1024;

// ── CRC-32 (ISO 3309 / IEEE 802.3) via pre-computed table ──────────────

static quint32 crc32_update(quint32 crc, const char *data, qint64 len)
{
    static quint32 table[256];
    static bool tableReady = false;
    if (!tableReady) {
        for (int i = 0; i < 256; ++i) {
            quint32 v = static_cast<quint32>(i);
            for (int j = 0; j < 8; ++j)
                v = (v >> 1) ^ (v & 1 ? 0xEDB88320 : 0);
            table[i] = v;
        }
        tableReady = true;
    }

    for (qint64 i = 0; i < len; ++i)
        crc = table[(crc ^ static_cast<quint8>(data[i])) & 0xFF] ^ (crc >> 8);
    return crc;
}

// ── Public API ──────────────────────────────────────────────────────────

RomHashes RomHasher::computeAll(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    QCryptographicHash sha1(QCryptographicHash::Sha1);
    quint32 crc = 0xFFFFFFFF;
    qint64 size = 0;

    while (!file.atEnd()) {
        QByteArray chunk = file.read(kChunkSize);
        if (chunk.isEmpty())
            break;
        crc = crc32_update(crc, chunk.constData(), chunk.size());
        sha1.addData(chunk);
        size += chunk.size();
    }

    RomHashes h;
    h.crc32    = QString::number(crc ^ 0xFFFFFFFF, 16).rightJustified(8, '0');
    h.sha1     = sha1.result().toHex();
    h.fileSize = size;
    return h;
}

QString RomHasher::computeCRC32(const QString &filePath)
{
    return computeAll(filePath).crc32;
}

QString RomHasher::computeSHA1(const QString &filePath)
{
    return computeAll(filePath).sha1;
}

// ── ROM ID generation  SYSTEM-REGION-TITLE-XXXXX ────────────────────────

static QString normaliseSystem(const QString &raw)
{
    QString s = raw.trimmed().toUpper();
    static const QHash<QString, QString> map{
        {"SUPER NINTENDO", "SNES"},
        {"NINTENDO ENTERTAINMENT SYSTEM", "NES"},
        {"GAME BOY ADVANCE", "GBA"},
        {"GAME BOY COLOR", "GBC"},
        {"GAME BOY", "GB"},
        {"NINTENDO 64", "N64"},
        {"NINTENDO DS", "NDS"},
        {"NINTENDO 3DS", "3DS"},
        {"PLAYSTATION", "PS1"},
        {"PLAYSTATION 2", "PS2"},
        {"PLAYSTATION 3", "PS3"},
        {"PLAYSTATION 4", "PS4"},
        {"PLAYSTATION PORTABLE", "PSP"},
        {"PLAYSTATION VITA", "PSV"},
        {"SEGA GENESIS", "GENESIS"},
        {"SEGA MEGA DRIVE", "GENESIS"},
        {"SEGA DREAMCAST", "DREAMCAST"},
        {"SEGA SATURN", "SATURN"},
        {"MICROSOFT XBOX", "XBOX"},
        {"XBOX 360", "XBOX360"},
        {"XBOX ONE", "XBOXONE"},
        {"NINTENDO GAMECUBE", "GC"},
        {"NINTENDO WII", "WII"},
        {"NINTENDO WII U", "WIIU"},
        {"NINTENDO SWITCH", "SWITCH"},
    };
    auto it = map.find(s);
    if (it != map.end())
        return it.value();
    QString out;
    for (const QChar &c : s) {
        if (c.isLetterOrNumber())
            out += c;
    }
    return out;
}

static QString normaliseRegion(const QString &region)
{
    // Check for serial number pattern (e.g. "SCUS-97101" → "SCUS")
    static QRegularExpression serialRe(R"(([A-Z]{2,4})-?\d{3,5})",
                                        QRegularExpression::CaseInsensitiveOption);
    auto m = serialRe.match(region.trimmed().toUpper());
    if (m.hasMatch()) {
        QString serial = m.captured(1).toUpper();
        QString letters;
        for (const QChar &c : serial) {
            if (c.isLetter())
                letters += c;
        }
        if (!letters.isEmpty())
            return letters.left(4);
    }

    QString r = region.trimmed().toUpper();
    static const QHash<QString, QString> map{
        {"UNITED STATES", "USA"},
        {"US", "USA"},
        {"AMERICA", "USA"},
        {"NORTH AMERICA", "USA"},
        {"NTSC", "USA"},
        {"JAPAN", "JPN"},
        {"JAPANESE", "JPN"},
        {"JP", "JPN"},
        {"EUROPE", "EUR"},
        {"EUROPEAN", "EUR"},
        {"PAL", "EUR"},
        {"UK", "EUR"},
        {"WORLD", "WORLD"},
        {"ASIA", "ASIA"},
        {"KOREA", "KOR"},
        {"CHINA", "CHN"},
        {"BRAZIL", "BRA"},
        {"AUSTRALIA", "AUS"},
    };
    auto it = map.find(r);
    if (it != map.end())
        return it.value();
    return r.left(4);
}

static QString normaliseTitle(const QString &raw)
{
    QString t = raw.toUpper();
    QString out;
    for (const QChar &c : t) {
        if (c.isLetterOrNumber())
            out += c;
    }
    return out;
}

QString RomHasher::generateRomId(const QString &platform,
                                 const QString &region,
                                 const QString &title,
                                 const QString &sha1Hash)
{
    QString systemPart  = normaliseSystem(platform);
    QString regionPart  = normaliseRegion(region);
    QString titleSlug   = normaliseTitle(title);

    QString suffix;
    if (!sha1Hash.isEmpty())
        suffix = sha1Hash.left(5).toUpper();
    else
        suffix = QString(QCryptographicHash::hash(title.toUtf8(),
                         QCryptographicHash::Sha1).toHex().left(5)).toUpper();

    return QString("%1-%2-%3-%4")
        .arg(systemPart, regionPart, titleSlug, suffix);
}
