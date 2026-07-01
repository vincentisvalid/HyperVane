# Source file for Account Widgets
# src/ui/AccountWidget.cpp

#include "AccountWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>

namespace HyperVane {

AccountWidget::AccountWidget(QWidget *parent) : QWidget(parent) {
    // Apply the main theme stylesheet dynamically
    this->setStyleSheet("QWidget { background-color: #141414; }"); 
    setupUi();
}

void AccountWidget::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setAlignment(Qt::AlignCenter);

    QLabel* headerLabel = new QLabel("Welcome to HyperVane", this);
    headerLabel->setObjectName("titleLabel"); // Use the defined style class for styling
    headerLabel->setStyleSheet("font-size: 36px; margin-bottom: 40px; color: #e50914;");
    mainLayout->addWidget(headerLabel);

    // Input Fields Layout (Username and Password)
    QHBoxLayout* inputLayout = new QHBoxLayout();
    inputLayout->addWidget(new QLabel("Username:", this));
    usernameInput = new QLineEdit(this);
    usernameInput->setPlaceholderText("Enter your username");
    inputLayout->addWidget(usernameInput);

    inputLayout->addWidget(new QLabel("Password:", this));
    passwordInput = new QLineEdit(this);
    passwordInput->setPasswordMask(true);
    inputLayout->addWidget(passwordInput);

    mainLayout->addLayout(inputLayout);

    // Buttons Layout (Login and Create)
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    loginButton = new QPushButton("Login", this);
    createButton = new QPushButton("Create Account", this);
    buttonLayout->addWidget(loginButton);
    buttonLayout->addWidget(createButton);

    mainLayout->addLayout(buttonLayout);
}

bool AccountWidget::attemptLogin(const QString& user, const QString& pass) {
    // Placeholder logic: Mocking a successful login attempt
    qDebug() << "Attempting login for:", user;
    if (user.isEmpty() or pass.isEmpty()) {
        statusLabel->setText("Username and password cannot be empty.");
        return false;
    }
    // In a real scenario, this would make an IPC request to the back-end Auth Service
    qDebug() << "Login successful! Proceeding...";
    emit login_success(true); // Signal success to parent widget
    return true;
}

} // namespace HyperVane