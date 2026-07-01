#include "InGameMenu.h"

#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

InGameMenu::InGameMenu(const QString &gameTitle, QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setStyleSheet("InGameMenu { background: rgba(245, 245, 245, 245); }");

    auto *root = new QVBoxLayout(this);
    root->setAlignment(Qt::AlignCenter);

    auto *card = new QFrame;
    card->setFixedWidth(400);
    card->setStyleSheet(
        "QFrame { background: white; border-radius: 16px; "
        "border: 1px solid #e0e0e0; }");
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setSpacing(0);
    cardLayout->setContentsMargins(32, 32, 32, 24);

    auto *headerLabel = new QLabel("GAME PAUSED");
    headerLabel->setAlignment(Qt::AlignCenter);
    headerLabel->setStyleSheet(
        "font-size: 13px; font-weight: 700; color: #999; "
        "letter-spacing: 4px;");
    cardLayout->addWidget(headerLabel);

    cardLayout->addSpacing(8);

    m_gameLabel = new QLabel(gameTitle);
    m_gameLabel->setAlignment(Qt::AlignCenter);
    m_gameLabel->setStyleSheet(
        "font-size: 18px; font-weight: 600; color: #1a1a1a; "
        "padding: 0 0 16px 0;");
    m_gameLabel->setWordWrap(true);
    cardLayout->addWidget(m_gameLabel);

    auto *sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet("background: #eee;");
    cardLayout->addWidget(sep);

    cardLayout->addSpacing(16);

    auto *resumeBtn  = createButton("  Resume Game");
    auto *saveBtn    = createButton("  Save && Close");
    auto *closeBtn   = createButton("  Close Game");
    auto *homeBtn    = createButton("  Go Home");
    auto *exitBtn    = createButton("  Exit HyperVane");

    cardLayout->addWidget(resumeBtn);
    cardLayout->addSpacing(6);
    cardLayout->addWidget(saveBtn);
    cardLayout->addSpacing(6);
    cardLayout->addWidget(closeBtn);
    cardLayout->addSpacing(6);
    cardLayout->addWidget(homeBtn);
    cardLayout->addSpacing(6);
    cardLayout->addWidget(exitBtn);

    cardLayout->addSpacing(16);

    m_hintLabel = new QLabel("ESC to resume");
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setStyleSheet(
        "font-size: 12px; color: #aaa;");
    cardLayout->addWidget(m_hintLabel);

    root->addWidget(card, 0, Qt::AlignCenter);

    connect(resumeBtn, &QPushButton::clicked, this, &InGameMenu::resumeGame);
    connect(saveBtn,   &QPushButton::clicked, this, &InGameMenu::saveAndClose);
    connect(closeBtn,  &QPushButton::clicked, this, &InGameMenu::closeGame);
    connect(homeBtn,   &QPushButton::clicked, this, &InGameMenu::goHome);
    connect(exitBtn,   &QPushButton::clicked, this, &InGameMenu::exitHyperVane);
}

void InGameMenu::setGameTitle(const QString &title)
{
    m_gameLabel->setText(title);
}

QPushButton *InGameMenu::createButton(const QString &text)
{
    auto *btn = new QPushButton(text);
    btn->setFixedHeight(48);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(
        "QPushButton {"
        "  background: #f5f5f5;"
        "  border: 1px solid #e0e0e0;"
        "  border-radius: 10px;"
        "  font-size: 15px; color: #1a1a1a;"
        "  text-align: left; padding: 0 16px;"
        "}"
        "QPushButton:hover {"
        "  background: #e8f0fe;"
        "  border-color: #0066cc;"
        "  color: #0066cc;"
        "}");
    return btn;
}

void InGameMenu::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        emit resumeGame();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void InGameMenu::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    setFocus();
}
