#pragma once

#include <QDialog>
#include "AccountManager.h"

class QLabel;
class QLineEdit;

class RegisterDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RegisterDialog(QWidget *parent = nullptr);
    Account createdAccount() const { return m_created; }

private slots:
    void onUsernameChanged(const QString &text);
    void onRegisterClicked();

private:
    QLabel    *m_avatarLabel  = nullptr;
    QLineEdit *m_usernameEdit = nullptr;
    QLineEdit *m_emailEdit    = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QLineEdit *m_confirmEdit  = nullptr;
    QLabel    *m_errorLabel   = nullptr;
    Account    m_created;
};
