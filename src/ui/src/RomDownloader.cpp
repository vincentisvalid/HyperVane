#include "RomDownloader.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrlQuery>

static const char *kSearchUrl  = "http://localhost:8080/api/roms/search";
static const char *kDownloadUrl = "http://localhost:8080/api/roms/download";

RomDownloader &RomDownloader::instance()
{
    static RomDownloader inst;
    return inst;
}

RomDownloader::RomDownloader(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

void RomDownloader::searchRom(const QString &query, const QString &platform)
{
    QUrl url(kSearchUrl);
    QUrlQuery q;
    q.addQueryItem("q", query);
    if (!platform.isEmpty())
        q.addQueryItem("platform", platform);
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader("Accept", "application/json");

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            // Fall back to mock server
            if (reply->url().toString().contains("localhost:8080")) {
                emit searchError("Server unavailable. Using local results.");
            }
            return;
        }

        QByteArray data = reply->readAll();
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error != QJsonParseError::NoError) {
            emit searchError("Invalid search response");
            return;
        }

        QVector<RomInfo> results;
        QJsonArray arr = doc.isArray() ? doc.array() : doc.object()["results"].toArray();
        for (const auto &v : arr) {
            QJsonObject o = v.toObject();
            RomInfo ri;
            ri.id       = o["id"].toString();
            ri.title    = o["title"].toString();
            ri.platform = o["platform"].toString();
            ri.region   = o["region"].toString();
            ri.filePath = o["file_path"].toString();
            ri.verified = o["verified"].toBool();
            results.push_back(ri);
        }
        emit searchResults(results);
    });
}

void RomDownloader::downloadRom(const QString &url, const QString &destPath, const QString &romId)
{
    QDir dir = QFileInfo(destPath).absoluteDir();
    if (!dir.exists())
        dir.mkpath(".");

    QString rid = romId.isEmpty() ? QString::number(qHash(url), 16) : romId;

    // Prevent duplicate downloads
    if (m_downloads.contains(rid)) return;

    QNetworkRequest req{QUrl(url)};
    req.setRawHeader("Accept", "application/octet-stream");

    QNetworkReply *reply = m_nam->get(req);
    auto *file = new QFile(destPath);

    if (!file->open(QIODevice::WriteOnly)) {
        delete file;
        emit downloadError(rid, "Cannot open destination file");
        return;
    }

    m_downloads[rid] = {rid, reply, file, destPath};

    connect(reply, &QNetworkReply::readyRead, this, [this, reply, rid]() {
        auto it = m_downloads.find(rid);
        if (it != m_downloads.end() && it.value().file) {
            it.value().file->write(reply->readAll());
        }
    });

    connect(reply, &QNetworkReply::downloadProgress, this, [this, rid](qint64 recv, qint64 total) {
        emit downloadProgress(rid, recv, total);
    });

    connect(reply, &QNetworkReply::finished, this, [this, rid]() {
        auto it = m_downloads.find(rid);
        if (it == m_downloads.end()) return;

        DownloadJob job = it.value();
        m_downloads.erase(it);

        if (job.reply->error() == QNetworkReply::NoError) {
            if (job.file) {
                job.file->write(job.reply->readAll());
                job.file->close();
            }
            emit downloadComplete(rid, job.destPath);
        } else {
            if (job.file) {
                job.file->remove();
                delete job.file;
            }
            emit downloadError(rid, job.reply->errorString());
        }
        job.reply->deleteLater();
    });
}

void RomDownloader::cancelDownload(const QString &romId)
{
    if (romId.isEmpty()) {
        // Cancel all
        for (auto it = m_downloads.begin(); it != m_downloads.end(); ++it) {
            it.value().reply->abort();
            if (it.value().file) {
                it.value().file->remove();
                delete it.value().file;
            }
            it.value().reply->deleteLater();
        }
        m_downloads.clear();
    } else {
        auto it = m_downloads.find(romId);
        if (it != m_downloads.end()) {
            it.value().reply->abort();
            if (it.value().file) {
                it.value().file->remove();
                delete it.value().file;
            }
            it.value().reply->deleteLater();
            m_downloads.erase(it);
        }
    }
}
