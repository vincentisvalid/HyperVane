#include "SocoliteEngine.h"

#include <QDir>
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

SocoliteEngine &SocoliteEngine::instance()
{
    static SocoliteEngine inst;
    return inst;
}

SocoliteEngine::SocoliteEngine()
    : m_socket(new QLocalSocket(this))
    , m_heartbeat(new QTimer(this))
{
    connect(m_socket, &QLocalSocket::connected, this, &SocoliteEngine::onConnected);
    connect(m_socket, &QLocalSocket::disconnected, this, &SocoliteEngine::onDisconnected);
    connect(m_socket, &QLocalSocket::readyRead, this, &SocoliteEngine::onReadyRead);
    connect(m_heartbeat, &QTimer::timeout, this, &SocoliteEngine::onHeartbeat);
}

// ── Connection ──────────────────────────────────────────────────────────────

void SocoliteEngine::connectToServer(const QString &socketPath)
{
    if (m_connected) return;
    m_socket->connectToServer(socketPath);
}

void SocoliteEngine::disconnect()
{
    m_heartbeat->stop();
    m_socket->disconnectFromServer();
}

bool SocoliteEngine::isConnected() const { return m_connected; }

void SocoliteEngine::setLocalUser(const QString &id, const QString &username)
{
    m_localId = id;
    m_localUsername = username;
}

// ── Friends ─────────────────────────────────────────────────────────────────

QVector<SocoliteUser> SocoliteEngine::friends() const { return m_friends; }

QVector<SocoliteUser> SocoliteEngine::onlineFriends() const
{
    QVector<SocoliteUser> result;
    for (const auto &f : m_friends) {
        if (f.status != "offline")
            result.push_back(f);
    }
    return result;
}

void SocoliteEngine::addFriend(const QString &username)
{
    QJsonObject msg;
    msg["type"] = "add_friend";
    msg["username"] = username;
    sendJson(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void SocoliteEngine::removeFriend(const QString &userId)
{
    QJsonObject msg;
    msg["type"] = "remove_friend";
    msg["user_id"] = userId;
    sendJson(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void SocoliteEngine::setStatus(const QString &status, const QString &gameTitle)
{
    QJsonObject msg;
    msg["type"] = "set_status";
    msg["status"] = status;
    if (!gameTitle.isEmpty())
        msg["game_title"] = gameTitle;
    sendJson(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

// ── Chat ────────────────────────────────────────────────────────────────────

QVector<SocoliteMessage> SocoliteEngine::messages(const QString &friendId) const
{
    return m_chatHistory.value(friendId);
}

void SocoliteEngine::sendMessage(const QString &friendId, const QString &text, const QByteArray &imageData)
{
    SocoliteMessage msg;
    msg.id = nextMsgId();
    msg.senderId = m_localId;
    msg.senderName = m_localUsername;
    msg.text = text;
    msg.imageData = imageData;
    msg.timestamp = QDateTime::currentDateTime();
    msg.isLocal = true;

    m_chatHistory[friendId].push_back(msg);
    emit messageReceived(msg);

    // Send over wire
    QJsonObject json;
    json["type"] = "chat_message";
    json["to_id"] = friendId;
    json["text"] = text;
    if (!imageData.isEmpty())
        json["image"] = QString::fromLatin1(imageData.toBase64());
    sendJson(QJsonDocument(json).toJson(QJsonDocument::Compact));
}

// ── Search ──────────────────────────────────────────────────────────────────

void SocoliteEngine::searchUsers(const QString &query)
{
    QJsonObject msg;
    msg["type"] = "search_users";
    msg["query"] = query;
    sendJson(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

// ── Activity ────────────────────────────────────────────────────────────────

QVector<SocoliteActivity> SocoliteEngine::activityFeed() const
{
    return m_activityFeed;
}

void SocoliteEngine::postActivity(SocoliteActivity::Type type, const QString &message, const QString &detail, const QByteArray &imageData)
{
    SocoliteActivity act;
    act.type = type;
    act.userId = m_localId;
    act.username = m_localUsername;
    act.message = message;
    act.detail = detail;
    act.imageData = imageData;
    act.timestamp = QDateTime::currentDateTime();
    m_activityFeed.insert(m_activityFeed.begin(), act);
    if (m_activityFeed.size() > 100)
        m_activityFeed.resize(100);
    emit activityAdded(act);

    QJsonObject json;
    json["type"] = "post_activity";
    json["activity_type"] = static_cast<int>(type);
    json["message"] = message;
    json["detail"] = detail;
    if (!imageData.isEmpty())
        json["image"] = QString::fromLatin1(imageData.toBase64());
    sendJson(QJsonDocument(json).toJson(QJsonDocument::Compact));
}

// ── Avatar ──────────────────────────────────────────────────────────────────

void SocoliteEngine::setAvatar(const QByteArray &imageData)
{
    m_localAvatar = imageData;
    emit avatarChanged();

    QJsonObject json;
    json["type"] = "set_avatar";
    json["image"] = QString::fromLatin1(imageData.toBase64());
    sendJson(QJsonDocument(json).toJson(QJsonDocument::Compact));
}

QByteArray SocoliteEngine::avatar() const { return m_localAvatar; }

// ── Socket handlers ─────────────────────────────────────────────────────────

void SocoliteEngine::onConnected()
{
    m_connected = true;
    m_heartbeat->start(30000); // 30s heartbeat
    emit connected();
}

void SocoliteEngine::onDisconnected()
{
    m_connected = false;
    m_heartbeat->stop();
    emit disconnected();
}

void SocoliteEngine::onReadyRead()
{
    m_readBuf.append(m_socket->readAll());
    while (true) {
        int nl = m_readBuf.indexOf('\n');
        if (nl < 0) break;
        QByteArray line = m_readBuf.left(nl);
        m_readBuf.remove(0, nl + 1);
        if (!line.trimmed().isEmpty())
            handleServerMessage(line);
    }
}

void SocoliteEngine::onHeartbeat()
{
    QJsonObject msg;
    msg["type"] = "heartbeat";
    sendJson(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void SocoliteEngine::sendJson(const QByteArray &payload)
{
    if (!m_connected || !m_socket->isOpen()) return;
    m_socket->write(payload + "\n");
    m_socket->flush();
}

void SocoliteEngine::handleServerMessage(const QByteArray &data)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) return;

    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();

    if (type == "friends_list") {
        m_friends.clear();
        QJsonArray arr = obj["friends"].toArray();
        for (const auto &v : arr) {
            QJsonObject f = v.toObject();
            SocoliteUser u;
            u.id = f["id"].toString();
            u.username = f["username"].toString();
            u.displayName = f["display_name"].toString();
            u.avatarPath = f["avatar_path"].toString();
            u.status = f["status"].toString();
            u.gameTitle = f["game_title"].toString();
            u.isFriend = true;
            m_friends.push_back(u);
        }
        emit friendsListChanged();
    } else if (type == "friend_added") {
        QJsonObject f = obj["user"].toObject();
        SocoliteUser u;
        u.id = f["id"].toString();
        u.username = f["username"].toString();
        u.displayName = f["display_name"].toString();
        u.avatarPath = f["avatar_path"].toString();
        u.status = f["status"].toString();
        u.isFriend = true;
        m_friends.push_back(u);
        emit friendAdded(u);
        emit friendsListChanged();
    } else if (type == "friend_removed") {
        QString uid = obj["user_id"].toString();
        m_friends.erase(std::remove_if(m_friends.begin(), m_friends.end(),
            [&](const SocoliteUser &u) { return u.id == uid; }), m_friends.end());
        emit friendRemoved(uid);
        emit friendsListChanged();
    } else if (type == "status_changed") {
        QString uid = obj["user_id"].toString();
        QString status = obj["status"].toString();
        QString game = obj["game_title"].toString();
        for (auto &f : m_friends) {
            if (f.id == uid) {
                f.status = status;
                f.gameTitle = game;
                break;
            }
        }
        emit friendStatusChanged(uid, status, game);
        emit friendsListChanged();
    } else if (type == "chat_message") {
        SocoliteMessage msg;
        msg.id = obj["msg_id"].toString();
        msg.senderId = obj["from_id"].toString();
        msg.senderName = obj["from_name"].toString();
        msg.text = obj["text"].toString();
        msg.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODate);
        QString b64 = obj["image"].toString();
        if (!b64.isEmpty())
            msg.imageData = QByteArray::fromBase64(b64.toLatin1());

        QString fromId = msg.senderId;
        m_chatHistory[fromId].push_back(msg);
        emit messageReceived(msg);
    } else if (type == "search_results") {
        QVector<SocoliteUser> results;
        QJsonArray arr = obj["results"].toArray();
        for (const auto &v : arr) {
            QJsonObject u = v.toObject();
            SocoliteUser user;
            user.id = u["id"].toString();
            user.username = u["username"].toString();
            user.displayName = u["display_name"].toString();
            user.avatarPath = u["avatar_path"].toString();
            user.status = u["status"].toString();
            user.isFriend = u["is_friend"].toBool();
            results.push_back(user);
        }
        emit userSearchResults(results);
    } else if (type == "activity") {
        SocoliteActivity act;
        act.type = static_cast<SocoliteActivity::Type>(obj["activity_type"].toInt());
        act.userId = obj["user_id"].toString();
        act.username = obj["username"].toString();
        act.message = obj["message"].toString();
        act.detail = obj["detail"].toString();
        act.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODate);
        QString b64 = obj["image"].toString();
        if (!b64.isEmpty())
            act.imageData = QByteArray::fromBase64(b64.toLatin1());
        m_activityFeed.insert(m_activityFeed.begin(), act);
        if (m_activityFeed.size() > 100)
            m_activityFeed.resize(100);
        emit activityAdded(act);
    } else if (type == "error") {
        emit connectionError(obj["message"].toString());
    }
}

void SocoliteEngine::dispatchMessage(const QByteArray &) {}
