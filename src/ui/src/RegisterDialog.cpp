#include "RegisterDialog.h"

#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

static const char kLightTheme[] = R"(
    QDialog { background: #f5f5f5; }
    QLineEdit {
        background: white;
        color: #1a1a1a;
        border: 2px solid #ddd;
        border-radius: 8px;
        padding: 10px 16px;
        font-size: 14px;
        selection-background-color: #0066cc;
    }
    QLineEdit:focus { border-color: #0066cc; }
    QLineEdit::placeholder { color: #999; }
)";

RegisterDialog::RegisterDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("HyperVane — Create Profile");
    setModal(true);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    if (parent)
        resize(parent->size());
    setStyleSheet(kLightTheme);

    auto *layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(14);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *title = new QLabel("Create your profile");
    title->setStyleSheet("font-size: 32px; font-weight: 700; color: #1a1a1a; letter-spacing: -0.5px;");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto *sub = new QLabel("You can change this later in Settings.");
    sub->setStyleSheet("font-size: 16px; color: #666;");
    sub->setAlignment(Qt::AlignCenter);
    layout->addWidget(sub);

    // ── Avatar preview ────────────────────────────────────────────────────
    m_avatarLabel = new QLabel;
    m_avatarLabel->setFixedSize(80, 80);
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    const QPixmap blank = AccountManager::generateAvatar("?");
    m_avatarLabel->setPixmap(blank.scaled(80, 80, Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation));
    layout->addWidget(m_avatarLabel, 0, Qt::AlignCenter);

    // ── Fields ────────────────────────────────────────────────────────────
    auto *form = new QWidget;
    auto *formLayout = new QVBoxLayout(form);
    formLayout->setAlignment(Qt::AlignCenter);
    formLayout->setSpacing(12);
    formLayout->setContentsMargins(0, 0, 0, 0);

    m_usernameEdit = new QLineEdit;
    m_usernameEdit->setPlaceholderText("Username");
    m_usernameEdit->setFixedWidth(320);
    connect(m_usernameEdit, &QLineEdit::textChanged,
            this, &RegisterDialog::onUsernameChanged);

    m_emailEdit = new QLineEdit;
    m_emailEdit->setPlaceholderText("Email (optional)");
    m_emailEdit->setFixedWidth(320);

    m_passwordEdit = new QLineEdit;
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText("Password (optional)");
    m_passwordEdit->setFixedWidth(320);

    m_confirmEdit = new QLineEdit;
    m_confirmEdit->setEchoMode(QLineEdit::Password);
    m_confirmEdit->setPlaceholderText("Confirm password");
    m_confirmEdit->setFixedWidth(320);

    m_errorLabel = new QLabel;
    m_errorLabel->setStyleSheet("color: #e50914; font-size: 14px;");
    m_errorLabel->setAlignment(Qt::AlignCenter);
    m_errorLabel->hide();

    auto *createBtn = new QPushButton("Create Profile");
    createBtn->setFixedWidth(320);
    createBtn->setStyleSheet(
        "QPushButton { background: #0066cc; color: white; border: none; "
        "border-radius: 8px; padding: 12px 24px; font-size: 15px; font-weight: 600; }"
        "QPushButton:hover { background: #0052a3; }"
        "QPushButton:pressed { background: #004d99; }");
    connect(createBtn, &QPushButton::clicked, this, &RegisterDialog::onRegisterClicked);
    connect(m_confirmEdit, &QLineEdit::returnPressed, this, &RegisterDialog::onRegisterClicked);

    formLayout->addWidget(m_usernameEdit, 0, Qt::AlignCenter);
    formLayout->addWidget(m_emailEdit,    0, Qt::AlignCenter);
    formLayout->addWidget(m_passwordEdit, 0, Qt::AlignCenter);
    formLayout->addWidget(m_confirmEdit,  0, Qt::AlignCenter);

    layout->addWidget(form, 0, Qt::AlignCenter);
    layout->addWidget(m_errorLabel, 0, Qt::AlignCenter);
    layout->addSpacing(4);
    layout->addWidget(createBtn, 0, Qt::AlignCenter);
}

void RegisterDialog::onUsernameChanged(const QString &text)
{
    const QString display = text.trimmed().isEmpty() ? "?" : text.trimmed();
    const QPixmap pm = AccountManager::generateAvatar(display, 80);
    m_avatarLabel->setPixmap(pm);
}

void RegisterDialog::onRegisterClicked()
{
    const QString username = m_usernameEdit->text().trimmed();
    const QString email    = m_emailEdit->text().trimmed();
    const QString pass1    = m_passwordEdit->text();
    const QString pass2    = m_confirmEdit->text();

    if (username.isEmpty()) {
        m_errorLabel->setText("Username is required.");
        m_errorLabel->show();
        return;
    }
    if (!pass1.isEmpty() && pass1.length() < 8) {
        m_errorLabel->setText("Password must be at least 8 characters.");
        m_errorLabel->show();
        return;
    }
    if (pass1 != pass2) {
        m_errorLabel->setText("Passwords do not match.");
        m_errorLabel->show();
        return;
    }

    m_created = AccountManager::instance().createAccount(username, email, pass1);
    if (!m_created.isValid()) {
        m_errorLabel->setText("Username already taken.");
        m_errorLabel->show();
        return;
    }

    accept();
}
