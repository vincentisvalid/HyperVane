#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QHash>
#include <QLocalSocket>
#include <QTimer>
#include <memory>
#include "SocoliteTypes.h"

class SocoliteEngine : public QObject
{
    Q_OBJECT

public:
    static SocoliteEngine &instance();

    void connectToServer(const QString &socketPath = "/tmp/hypervane-social.sock");
    void disconnect();
    bool isConnected() const;

    // Friends
    QVector<SocoliteUser> friends() const;
    QVector<SocoliteUser> onlineFriends() const;
    void addFriend(const QString &username);
    void removeFriend(const QString &userId);
    void setStatus(const QString &status, const QString &gameTitle = {});

    // Chat
    QVector<SocoliteMessage> messages(const QString &friendId) const;
    void sendMessage(const QString &friendId, const QString &text, const QByteArray &imageData = {});

    // Search
    void searchUsers(const QString &query);

    // Activity
    QVector<SocoliteActivity> activityFeed() const;
    void postActivity(SocoliteActivity::Type type, const QString &message, const QString &detail = {}, const QByteArray &imageData = {});

    // Avatar
    void setAvatar(const QByteArray &imageData);
    QByteArray avatar() const;

    // Local user
    QString localUserId() const { return m_localId; }
    QString localUsername() const { return m_localUsername; }
    void setLocalUser(const QString &id, const QString &username);

signals:
    void connected();
    void disconnected();
    void connectionError(const QString &error);

    void friendsListChanged();
    void friendAdded(const SocoliteUser &user);
    void friendRemoved(const QString &userId);
    void friendStatusChanged(const QString &userId, const QString &status, const QString &gameTitle);

    void messageReceived(const SocoliteMessage &msg);
    void messagesLoaded(const QString &friendId);

    void userSearchResults(const QVector<SocoliteUser> &users);

    void activityAdded(const SocoliteActivity &activity);

    void avatarChanged();

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onHeartbeat();

private:
    SocoliteEngine();
    void dispatchMessage(const QByteArray &data);
    void sendJson(const QByteArray &payload);
    void handleServerMessage(const QByteArray &data);

    QLocalSocket *m_socket = nullptr;
    QByteArray m_readBuf;
    QTimer *m_heartbeat = nullptr;
    bool m_connected = false;

    QString m_localId;
    QString m_localUsername;
    QByteArray m_localAvatar;

    QVector<SocoliteUser> m_friends;
    QHash<QString, QVector<SocoliteMessage>> m_chatHistory; // friendId → messages
    QVector<SocoliteActivity> m_activityFeed;

    int m_nextMsgId = 1;
    QString nextMsgId() { return QString::number(m_nextMsgId++); }
};
