#pragma once

#include <QColor>
#include <QDateTime>
#include <QJsonObject>
#include <QObject>
#include <QPixmap>
#include <QString>
#include <QVector>

struct Account {
    QString   id;
    QString   username;
    QString   email;
    QColor    avatarColor;
    QString   avatarInitials;
    QString   passwordHash;
    QString   salt;
    QString   avatarBase64;
    QDateTime createdAt;

    QJsonObject       toJson()   const;
    static Account    fromJson(const QJsonObject &obj);
    bool              isValid()  const { return !id.isEmpty() && !username.isEmpty(); }
};

class AccountManager : public QObject
{
    Q_OBJECT
public:
    static AccountManager &instance();

    QVector<Account> accounts()       const;
    Account          currentAccount() const;
    bool             isLoggedIn()     const;

    Account createAccount(const QString &username,
                          const QString &email,
                          const QString &password);
    bool    login(const QString &accountId, const QString &password);
    void    logout();

    void    setAvatar(const QString &accountId, const QPixmap &pixmap);
    QPixmap avatarPixmap(const QString &accountId) const;

    static QPixmap generateAvatar(const QString &username, int size = 80);

signals:
    void accountsChanged();
    void loginStateChanged(bool loggedIn);

private:
    explicit AccountManager(QObject *parent = nullptr);
    void    load();
    void    save() const;
    static  QString hashPassword(const QString &salt, const QString &password);

    QVector<Account> m_accounts;
    Account          m_current;
    QString          m_dataPath;
};
