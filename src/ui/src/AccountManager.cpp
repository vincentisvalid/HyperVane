#include "AccountManager.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QUuid>
#include <QBuffer>

// ── Account serialisation ────────────────────────────────────────────────────

QJsonObject Account::toJson() const
{
    return QJsonObject{
        {"id",             id},
        {"username",       username},
        {"email",          email},
        {"avatarR",        avatarColor.red()},
        {"avatarG",        avatarColor.green()},
        {"avatarB",        avatarColor.blue()},
        {"avatarInitials", avatarInitials},
        {"passwordHash",   passwordHash},
        {"salt",           salt},
        {"avatarBase64",   avatarBase64},
        {"createdAt",      createdAt.toString(Qt::ISODate)},
    };
}

Account Account::fromJson(const QJsonObject &o)
{
    Account a;
    a.id             = o["id"].toString();
    a.username       = o["username"].toString();
    a.email          = o["email"].toString();
    a.avatarColor    = QColor(o["avatarR"].toInt(), o["avatarG"].toInt(), o["avatarB"].toInt());
    a.avatarInitials = o["avatarInitials"].toString();
    a.passwordHash   = o["passwordHash"].toString();
    a.salt           = o["salt"].toString();
    a.avatarBase64   = o["avatarBase64"].toString();
    a.createdAt      = QDateTime::fromString(o["createdAt"].toString(), Qt::ISODate);
    return a;
}

// ── AccountManager ───────────────────────────────────────────────────────────

AccountManager &AccountManager::instance()
{
    static AccountManager inst;
    return inst;
}

AccountManager::AccountManager(QObject *parent) : QObject(parent)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dir);
    m_dataPath = dir + "/accounts.json";
    load();
}

void AccountManager::load()
{
    QFile f(m_dataPath);
    if (!f.open(QFile::ReadOnly)) return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    m_accounts.clear();
    for (const QJsonValue &v : doc.array())
        m_accounts.append(Account::fromJson(v.toObject()));
}

void AccountManager::save() const
{
    QJsonArray arr;
    for (const Account &a : m_accounts)
        arr.append(a.toJson());
    QFile f(m_dataPath);
    if (f.open(QFile::WriteOnly | QFile::Truncate))
        f.write(QJsonDocument(arr).toJson());
}

QVector<Account> AccountManager::accounts()       const { return m_accounts; }
Account          AccountManager::currentAccount() const { return m_current; }
bool             AccountManager::isLoggedIn()     const { return m_current.isValid(); }

QString AccountManager::hashPassword(const QString &salt, const QString &password)
{
    QByteArray data = (salt + ":" + password).toUtf8();
    // 10k rounds of SHA-256 for key stretching
    QByteArray hash = data;
    for (int i = 0; i < 10000; ++i)
        hash = QCryptographicHash::hash(hash, QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex());
}

Account AccountManager::createAccount(const QString &username,
                                       const QString &email,
                                       const QString &password)
{
    for (const Account &a : m_accounts)
        if (a.username.toLower() == username.toLower())
            return {};

    Account a;
    a.id       = QUuid::createUuid().toString(QUuid::WithoutBraces);
    a.username = username.trimmed();
    a.email    = email.trimmed();
    a.createdAt = QDateTime::currentDateTime();

    // Deterministic avatar color from username hash
    const QByteArray nameHash = QCryptographicHash::hash(
        username.toUtf8(), QCryptographicHash::Sha256);
    a.avatarColor = QColor::fromHsl(
        (quint8)nameHash[0] * 360 / 255,
        120 + (quint8)nameHash[1] % 80,
        100 + (quint8)nameHash[2] % 60);

    // Initials: up to 2 chars
    const QStringList parts = username.split(' ', Qt::SkipEmptyParts);
    if (parts.size() >= 2)
        a.avatarInitials = QString(parts[0][0]).toUpper() + QString(parts[1][0]).toUpper();
    else if (!username.isEmpty())
        a.avatarInitials = QString(username[0]).toUpper();

    // Random 32-byte hex salt
    QByteArray rawSalt(32, '\0');
    QRandomGenerator::global()->fillRange(
        reinterpret_cast<quint32*>(rawSalt.data()),
        rawSalt.size() / 4);
    a.salt = QString::fromLatin1(rawSalt.toHex());
    a.passwordHash = password.isEmpty() ? QString() : hashPassword(a.salt, password);

    m_accounts.append(a);
    save();
    m_current = a;
    emit accountsChanged();
    emit loginStateChanged(true);
    return a;
}

bool AccountManager::login(const QString &accountId, const QString &password)
{
    for (const Account &a : m_accounts) {
        if (a.id != accountId) continue;
        // Allow passwordless accounts
        if (a.passwordHash.isEmpty() || hashPassword(a.salt, password) == a.passwordHash) {
            m_current = a;
            emit loginStateChanged(true);
            return true;
        }
        return false;
    }
    return false;
}

void AccountManager::logout()
{
    m_current = {};
    emit loginStateChanged(false);
}

void AccountManager::setAvatar(const QString &accountId, const QPixmap &pixmap)
{
    for (Account &a : m_accounts) {
        if (a.id != accountId) continue;
        QByteArray ba;
        QBuffer buf(&ba);
        buf.open(QIODevice::WriteOnly);
        pixmap.save(&buf, "PNG");
        a.avatarBase64 = QString::fromLatin1(ba.toBase64());
        if (m_current.id == accountId)
            m_current = a;
        save();
        emit accountsChanged();
        break;
    }
}

QPixmap AccountManager::avatarPixmap(const QString &accountId) const
{
    for (const Account &a : m_accounts) {
        if (a.id != accountId) continue;
        if (!a.avatarBase64.isEmpty()) {
            QPixmap pm;
            pm.loadFromData(QByteArray::fromBase64(a.avatarBase64.toLatin1()));
            if (!pm.isNull()) return pm;
        }
        return generateAvatar(a.username);
    }
    return {};
}

QPixmap AccountManager::generateAvatar(const QString &username, int size)
{
    // Deterministic HSL color from username
    const QByteArray h = QCryptographicHash::hash(username.toUtf8(),
                                                   QCryptographicHash::Sha256);
    const QColor bg = QColor::fromHsl(
        (quint8)h[0] * 360 / 255,
        120 + (quint8)h[1] % 80,
        100 + (quint8)h[2] % 60);

    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    // Squircle background
    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, size, size), size * 0.3, size * 0.3);
    p.fillPath(path, bg);

    // Initials
    QString initials;
    const QStringList parts = username.split(' ', Qt::SkipEmptyParts);
    if (parts.size() >= 2)
        initials = QString(parts[0][0]).toUpper() + QString(parts[1][0]).toUpper();
    else if (!username.isEmpty())
        initials = QString(username[0]).toUpper();

    p.setPen(Qt::white);
    QFont f = p.font();
    f.setPointSizeF(size * 0.3);
    f.setWeight(QFont::Bold);
    p.setFont(f);
    p.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, initials);

    return pm;
}
