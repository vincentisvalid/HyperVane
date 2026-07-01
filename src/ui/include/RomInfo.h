#pragma once
#include <QString>
#include <QStringList>

struct RomInfo {
    QString id;
    QString title;
    QString platform;
    QString region;
    QString filePath;
    bool    verified = false;

    // Content hashes (from protobuf RomEntry)
    QString crc32;
    QString sha1;
    qint64  fileSize = 0;

    // Metadata (populated from IPC when available)
    QString description;
    QStringList screenshots;   // URLs or local paths to screenshot images
    QString releaseDate;
    QString developer;
    QString genre;
    QString romId;             // SYSTEM-REGION-TITLE-XXXXX
};
