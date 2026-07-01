#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QUrl>
#include "RomInfo.h"

class RomDownloader : public QObject
{
    Q_OBJECT

public:
    static RomDownloader &instance();

    void searchRom(const QString &query, const QString &platform = {});
    void downloadRom(const QString &url, const QString &destPath, const QString &romId = {});
    void cancelDownload(const QString &romId = {});

signals:
    void searchResults(const QVector<RomInfo> &results);
    void searchError(const QString &error);

    void downloadProgress(const QString &romId, qint64 received, qint64 total);
    void downloadComplete(const QString &romId, const QString &filePath);
    void downloadError(const QString &romId, const QString &error);

private:
    explicit RomDownloader(QObject *parent = nullptr);

    struct DownloadJob {
        QString romId;
        QNetworkReply *reply = nullptr;
        QFile *file = nullptr;
        QString destPath;
    };

    QNetworkAccessManager *m_nam = nullptr;
    QHash<QString, DownloadJob> m_downloads;
};
