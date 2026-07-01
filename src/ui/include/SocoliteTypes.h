#pragma once

#include <QString>
#include <QDateTime>
#include <QVector>
#include <QImage>
#include <QPixmap>
#include <QByteArray>

struct SocoliteUser {
    QString id;
    QString username;
    QString displayName;
    QString avatarPath;       // local path to avatar image
    QString status;           // "online", "away", "busy", "offline", "ingame"
    QString gameTitle;        // what they're playing (if ingame)
    QByteArray avatarData;    // raw PNG data for custom avatars
    bool isFriend = false;
};

struct SocoliteMessage {
    QString id;
    QString senderId;
    QString senderName;
    QString text;
    QByteArray imageData;     // raw PNG drawing data (PictoChat)
    QDateTime timestamp;
    bool isLocal = false;     // true if sent by this user
};

struct SocoliteActivity {
    enum Type {
        GameStarted,
        GameStopped,
        ScreenshotShared,
        FriendAdded,
        FriendOnline,
        FriendOffline,
        ArtworkShared,
        Achievement
    };
    Type type;
    QString userId;
    QString username;
    QString avatarPath;
    QString message;
    QString detail;           // extra info (game name, achievement name)
    QByteArray imageData;     // optional screenshot/artwork
    QDateTime timestamp;
};
