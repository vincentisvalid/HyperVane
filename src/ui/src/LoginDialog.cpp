#include "LoginDialog.h"
#include "RegisterDialog.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

static const char kLightTheme[] = R"(
    QDialog { background: #f5f5f5; }
    #title {
        color: #1a1a1a;
        font-size: 32px;
        font-weight: 700;
        letter-spacing: -0.5px;
    }
    #subtitle {
        color: #666;
        font-size: 16px;
    }
    QLineEdit {
        background: white;
        color: #1a1a1a;
        border: 2px solid #ddd;
        border-radius: 8px;
        padding: 10px 16px;
        font-size: 14px;
        selection-background-color: #0066cc;
    }
    QLineEdit:focus {
        border-color: #0066cc;
    }
    QLineEdit::placeholder { color: #999; }
)";

LoginDialog::LoginDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("HyperVane — Sign In");
    setModal(true);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    if (parent)
        resize(parent->size());
    setStyleSheet(kLightTheme);

    m_stack = new QStackedWidget(this);

    m_profilePage  = new QWidget;
    m_passwordPage = new QWidget;

    buildProfilePage();

    // ── Password page ───────────────────────────────────────────────────────
    auto *passLayout = new QVBoxLayout(m_passwordPage);
    passLayout->setAlignment(Qt::AlignCenter);
    passLayout->setSpacing(14);

    m_avatarLabel = new QLabel;
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    m_avatarLabel->setFixedSize(80, 80);

    m_nameLabel = new QLabel;
    m_nameLabel->setStyleSheet("font-size: 20px; font-weight: 600; color: #1a1a1a;");
    m_nameLabel->setAlignment(Qt::AlignCenter);

    m_passEdit = new QLineEdit;
    m_passEdit->setEchoMode(QLineEdit::Password);
    m_passEdit->setPlaceholderText("Password");
    m_passEdit->setFixedWidth(300);
    connect(m_passEdit, &QLineEdit::returnPressed, this, &LoginDialog::onLoginClicked);

    m_errorLabel = new QLabel;
    m_errorLabel->setStyleSheet("color: #e50914; font-size: 14px;");
    m_errorLabel->setAlignment(Qt::AlignCenter);
    m_errorLabel->hide();

    auto *loginBtn = new QPushButton("Sign In");
    loginBtn->setFixedWidth(300);
    loginBtn->setStyleSheet(
        "QPushButton { background: #0066cc; color: white; border: none; "
        "border-radius: 8px; padding: 12px 24px; font-size: 15px; font-weight: 600; }"
        "QPushButton:hover { background: #0052a3; }"
        "QPushButton:pressed { background: #004d99; }");
    connect(loginBtn, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);

    auto *backBtn = new QPushButton("← Back");
    backBtn->setFixedWidth(300);
    backBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #666; border: 1px solid #ccc; "
        "border-radius: 8px; padding: 12px 24px; font-size: 14px; }"
        "QPushButton:hover { background: rgba(0,102,204,0.08); border-color: #0066cc; color: #0066cc; }");
    connect(backBtn, &QPushButton::clicked, this, &LoginDialog::onBackClicked);

    passLayout->addStretch();
    passLayout->addWidget(m_avatarLabel, 0, Qt::AlignCenter);
    passLayout->addWidget(m_nameLabel,   0, Qt::AlignCenter);
    passLayout->addWidget(m_passEdit,    0, Qt::AlignCenter);
    passLayout->addWidget(m_errorLabel,  0, Qt::AlignCenter);
    passLayout->addWidget(loginBtn,      0, Qt::AlignCenter);
    passLayout->addWidget(backBtn,       0, Qt::AlignCenter);
    passLayout->addStretch();

    m_stack->addWidget(m_profilePage);
    m_stack->addWidget(m_passwordPage);
    m_stack->setCurrentWidget(m_profilePage);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(m_stack);
}

void LoginDialog::buildProfilePage()
{
    auto *layout = new QVBoxLayout(m_profilePage);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(24);

    auto *title = new QLabel("Who's playing?");
    title->setObjectName("title");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    layout->addSpacing(8);

    const QVector<Account> accounts = AccountManager::instance().accounts();
    auto *grid = new QWidget;
    auto *gridLayout = new QGridLayout(grid);
    gridLayout->setSpacing(16);

    const int cols = qMin(4, accounts.size());
    for (int i = 0; i < accounts.size(); ++i) {
        const Account &acc = accounts[i];
        auto *card = new QPushButton;
        card->setFixedSize(130, 150);
        card->setStyleSheet(R"(
            QPushButton {
                background-color: white;
                border: 2px solid #e0e0e0;
                border-radius: 16px;
                padding: 12px;
            }
            QPushButton:hover {
                border: 2px solid #0066cc;
                background-color: #f0f7ff;
            }
        )");

        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setAlignment(Qt::AlignCenter);
        cardLayout->setSpacing(8);

        auto *avatarLbl = new QLabel;
        QPixmap avatar = AccountManager::instance().avatarPixmap(acc.id);
        avatarLbl->setPixmap(avatar.scaled(72, 72, Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation));
        avatarLbl->setAlignment(Qt::AlignCenter);

        auto *nameLbl = new QLabel(acc.username);
        nameLbl->setAlignment(Qt::AlignCenter);
        nameLbl->setWordWrap(true);
        nameLbl->setStyleSheet("font-size: 13px; font-weight: 600; color: #333;");

        cardLayout->addWidget(avatarLbl);
        cardLayout->addWidget(nameLbl);

        const QString id = acc.id;
        connect(card, &QPushButton::clicked, this,
                [this, id] { onProfileClicked(id); });

        gridLayout->addWidget(card, i / qMax(cols, 1), i % qMax(cols, 1));
    }

    layout->addWidget(grid, 0, Qt::AlignCenter);

    layout->addSpacing(8);

    auto *addBtn = new QPushButton("+ New Profile");
    addBtn->setStyleSheet(
        "QPushButton { background: #0066cc; color: white; border: none; "
        "border-radius: 8px; padding: 12px 32px; font-size: 15px; font-weight: 600; }"
        "QPushButton:hover { background: #0052a3; }"
        "QPushButton:pressed { background: #004d99; }");
    connect(addBtn, &QPushButton::clicked, this, &LoginDialog::onAddAccountClicked);
    layout->addWidget(addBtn, 0, Qt::AlignCenter);
}

void LoginDialog::onProfileClicked(const QString &accountId)
{
    const QVector<Account> accs = AccountManager::instance().accounts();
    Account found;
    for (const Account &a : accs)
        if (a.id == accountId) { found = a; break; }
    if (!found.isValid()) return;

    if (found.passwordHash.isEmpty()) {
        // No password — log in immediately
        AccountManager::instance().login(accountId, QString());
        m_selected = found;
        accept();
        return;
    }

    m_pendingId = accountId;
    showPasswordPage(found);
}

void LoginDialog::showPasswordPage(const Account &account)
{
    QPixmap avatar = AccountManager::instance().avatarPixmap(account.id);
    m_avatarLabel->setPixmap(avatar.scaled(80, 80, Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation));
    m_nameLabel->setText(account.username);
    m_passEdit->clear();
    m_errorLabel->hide();
    m_stack->setCurrentWidget(m_passwordPage);
    m_passEdit->setFocus();
}

void LoginDialog::onLoginClicked()
{
    if (AccountManager::instance().login(m_pendingId, m_passEdit->text())) {
        m_selected = AccountManager::instance().currentAccount();
        accept();
    } else {
        m_errorLabel->setText("Incorrect password.");
        m_errorLabel->show();
        m_passEdit->selectAll();
        m_passEdit->setFocus();
    }
}

void LoginDialog::onAddAccountClicked()
{
    RegisterDialog reg(this);
    if (reg.exec() == QDialog::Accepted) {
        m_selected = reg.createdAccount();
        accept();
    }
}

void LoginDialog::onBackClicked()
{
    m_stack->setCurrentWidget(m_profilePage);
}
