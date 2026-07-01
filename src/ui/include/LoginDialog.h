#pragma once

#include <QDialog>
#include "AccountManager.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;

class LoginDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LoginDialog(QWidget *parent = nullptr);
    Account selectedAccount() const { return m_selected; }

private slots:
    void onProfileClicked(const QString &accountId);
    void onLoginClicked();
    void onAddAccountClicked();
    void onBackClicked();

private:
    void buildProfilePage();
    void showPasswordPage(const Account &account);

    QStackedWidget *m_stack        = nullptr;
    QWidget        *m_profilePage  = nullptr;
    QWidget        *m_passwordPage = nullptr;
    QLineEdit      *m_passEdit     = nullptr;
    QLabel         *m_avatarLabel  = nullptr;
    QLabel         *m_nameLabel    = nullptr;
    QLabel         *m_errorLabel   = nullptr;
    QString         m_pendingId;
    Account         m_selected;
};
