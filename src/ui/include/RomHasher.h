#pragma once

#include <QString>
#include <QPair>
#include <QHash>

struct RomHashes {
    QString crc32;
    QString sha1;
    qint64  fileSize = 0;
};

class RomHasher
{
public:
    static RomHashes computeAll(const QString &filePath);
    static QString    computeCRC32(const QString &filePath);
    static QString    computeSHA1(const QString &filePath);

    static QString generateRomId(const QString &platform,
                                 const QString &region,
                                 const QString &title,
                                 const QString &sha1Hash = {});
};
