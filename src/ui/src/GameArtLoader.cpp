#include "GameArtLoader.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLinearGradient>
#include <QNetworkReply>
#include <QPainter>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QUrlQuery>

static QColor colorFor(const QString &id)
{
    static const QVector<QColor> palette = {
        QColor(0x00, 0x66, 0xcc),
        QColor(0x00, 0x88, 0x44),
        QColor(0xcc, 0x44, 0x00),
        QColor(0x88, 0x22, 0xcc),
        QColor(0xcc, 0x22, 0x44),
        QColor(0x22, 0x66, 0x88),
        QColor(0x66, 0x44, 0x22),
        QColor(0x22, 0x88, 0x66),
    };
    uint h = qHash(id);
    return palette[h % palette.size()];
}

static QString initials(const QString &title)
{
    QStringList words = title.split(' ', Qt::SkipEmptyParts);
    if (words.isEmpty()) return "?";
    if (words.size() == 1)
        return words[0].left(2).toUpper();
    return words[0].left(1).toUpper() + words[1].left(1).toUpper();
}

GameArtLoader &GameArtLoader::instance()
{
    static GameArtLoader inst;
    return inst;
}

GameArtLoader::GameArtLoader(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    m_cacheDir = QDir(dataDir + "/art");
    m_cacheDir.mkpath(".");
}

QString GameArtLoader::cachePath(const QString &romId) const
{
    QByteArray hash = QCryptographicHash::hash(romId.toUtf8(), QCryptographicHash::Md5);
    return m_cacheDir.absoluteFilePath(QString::fromLatin1(hash.toHex()) + ".png");
}

bool GameArtLoader::loadFromDisk(const QString &romId, QPixmap &pixmap)
{
    QString path = cachePath(romId);
    if (QFile::exists(path)) {
        pixmap.load(path);
        return !pixmap.isNull();
    }
    return false;
}

void GameArtLoader::saveToDisk(const QString &romId, const QPixmap &pixmap)
{
    pixmap.save(cachePath(romId), "PNG");
}

QPixmap GameArtLoader::artFor(const QString &romId, const QString &title, const QString &platform)
{
    auto it = m_cache.find(romId);
    if (it != m_cache.end())
        return it.value();

    QPixmap pm;
    if (loadFromDisk(romId, pm)) {
        m_cache.insert(romId, pm);
        return pm;
    }

    // Generate instant placeholder art so the grid never shows empty cards
    QPixmap placeholder(210, 280);
    placeholder.fill(Qt::transparent);
    {
        QPainter p(&placeholder);
        p.setRenderHint(QPainter::Antialiasing);

        QColor c = colorFor(romId.isEmpty() ? title : romId);
        QLinearGradient grad(0, 0, 210, 280);
        grad.setColorAt(0.0, c.lighter(130));
        grad.setColorAt(1.0, c.darker(140));
        p.setBrush(grad);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(0, 0, 210, 280, 12, 12);

        p.setPen(Qt::white);
        QFont f = p.font();
        f.setPointSize(36);
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRect(0, 40, 210, 80), Qt::AlignCenter, initials(title));

        f.setPointSize(10);
        f.setBold(false);
        p.setFont(f);
        p.drawText(QRect(0, 130, 210, 40), Qt::AlignCenter, platform);
    }

    m_cache.insert(romId, placeholder);
    saveToDisk(romId, placeholder);

    // Kick off network fetch in background — will replace placeholder later
    if (!title.isEmpty())
        fetchFromNetwork(romId, title, platform);

    return placeholder;
}

void GameArtLoader::requestArt(const QString &platform, const QString &title, ArtCallback callback)
{
    QString romId = title + "|" + platform;
    QPixmap art = artFor(romId, title, platform);
    if (!art.isNull()) {
        callback(art);
        return;
    }
    // Art will arrive later — connect to the signal once
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(this, &GameArtLoader::artReady, this,
        [this, conn, romId, callback](const QString &readyId) {
            if (readyId == romId) {
                callback(artFor(romId, {}, {}));
                disconnect(*conn);
            }
        });
    prefetch(romId, title, platform);
}

void GameArtLoader::prefetch(const QString &romId, const QString &title, const QString &platform)
{
    if (title.isEmpty()) return;
    // Enqueue a background fetch even if placeholder already cached
    if (!loadFromDisk(romId, *new QPixmap) || m_cache.contains(romId))
        return;
    fetchFromNetwork(romId, title, platform);
}

void GameArtLoader::fetchFromNetwork(const QString &romId, const QString &title, const QString &platform)
{
    QString query = title + " " + platform + " box art";
    QUrl url("https://www.googleapis.com/customsearch/v1");
    QUrlQuery params;
    params.addQueryItem("q", query);
    params.addQueryItem("searchType", "image");
    params.addQueryItem("cx", "009653096837215180394:0vjjiw4tia4");
    params.addQueryItem("key", "AIzaSyAS0tvg0yi5hC-MEnvRzLEXa721NwIjmM0");
    params.addQueryItem("num", "1");
    url.setQuery(params);

    QNetworkReply *reply = m_nam->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, romId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return;

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject obj = doc.object();
        auto items = obj["items"].toArray();
        if (items.isEmpty())
            return;

        QString imageUrl = items[0].toObject()["link"].toString();
        QNetworkReply *imgReply = m_nam->get(QNetworkRequest(QUrl(imageUrl)));
        connect(imgReply, &QNetworkReply::finished, this, [this, imgReply, romId]() {
            imgReply->deleteLater();
            if (imgReply->error() != QNetworkReply::NoError)
                return;

            QByteArray data = imgReply->readAll();
            QPixmap pm;
            if (pm.loadFromData(data)) {
                QPixmap scaled = pm.scaled(210, 280, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                m_cache.insert(romId, scaled);
                saveToDisk(romId, scaled);
                emit artReady(romId);
            }
        });
    });
}
