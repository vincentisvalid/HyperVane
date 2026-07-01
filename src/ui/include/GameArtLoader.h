#pragma once

#include <QPixmap>
#include <QString>
#include <QDir>
#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <functional>

class GameArtLoader : public QObject
{
    Q_OBJECT

public:
    static GameArtLoader &instance();

    QPixmap artFor(const QString &romId, const QString &title, const QString &platform);
    void prefetch(const QString &romId, const QString &title, const QString &platform);

    using ArtCallback = std::function<void(const QPixmap &)>;
    void requestArt(const QString &platform, const QString &title, ArtCallback callback);

signals:
    void artReady(const QString &romId);

private:
    explicit GameArtLoader(QObject *parent = nullptr);

    QString cachePath(const QString &romId) const;
    bool loadFromDisk(const QString &romId, QPixmap &pixmap);
    void saveToDisk(const QString &romId, const QPixmap &pixmap);
    void fetchFromNetwork(const QString &romId, const QString &title, const QString &platform);

    QDir m_cacheDir;
    QNetworkAccessManager *m_nam;
    QHash<QString, QPixmap> m_cache;
};
