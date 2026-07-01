#include "SocoliteChat.h"
#include "SocoliteCanvas.h"
#include "SocoliteEngine.h"

#include <QBuffer>
#include <QFrame>
#include <QHBoxLayout>
#include <QDateTime>
#include <QScrollBar>
#include <QTimer>
#include <QToolBar>
#include <QImage>

SocoliteChat::SocoliteChat(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void SocoliteChat::setupUI()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    // Header
    auto *header = new QLabel("Socolite Chat");
    header->setStyleSheet("font-size: 16px; font-weight: bold; color: #0066cc; padding: 4px 0;");
    layout->addWidget(header);

    // Message area
    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");

    m_messageArea = new QWidget;
    m_messageLayout = new QVBoxLayout(m_messageArea);
    m_messageLayout->setAlignment(Qt::AlignTop);
    m_messageLayout->setSpacing(6);
    m_messageLayout->addStretch();
    m_scrollArea->setWidget(m_messageArea);
    layout->addWidget(m_scrollArea, 1);

    // Drawing canvas (hidden by default)
    m_drawContainer = new QWidget;
    auto *drawLayout = new QVBoxLayout(m_drawContainer);
    drawLayout->setContentsMargins(0, 0, 0, 0);

    m_drawWidget = new SocoliteCanvas;
    m_drawWidget->setFixedHeight(200);

    auto *toolbar = new QWidget;
    auto *tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(0, 0, 0, 0);

    // Color buttons
    QVector<QColor> colors = {
        QColor("#000000"), QColor("#e50914"), QColor("#0066cc"),
        QColor("#00cc66"), QColor("#e5a00d"), QColor("#9b59b6"),
    };
    for (const auto &c : colors) {
        auto *btn = new QPushButton;
        btn->setFixedSize(22, 22);
        btn->setStyleSheet(QString("background: %1; border: 2px solid #555; border-radius: 11px;").arg(c.name()));
        connect(btn, &QPushButton::clicked, this, [this, c]() {
            m_drawWidget->setPenColor(c);
        });
        tbLayout->addWidget(btn);
    }

    auto *clearBtn = new QPushButton("Clear");
    clearBtn->setFixedHeight(22);
    clearBtn->setStyleSheet("font-size: 10px; padding: 0 6px;");
    connect(clearBtn, &QPushButton::clicked, m_drawWidget, &SocoliteCanvas::clearCanvas);
    tbLayout->addWidget(clearBtn);

    auto *sendDrawBtn = new QPushButton("Send Drawing");
    sendDrawBtn->setFixedHeight(22);
    sendDrawBtn->setStyleSheet("font-size: 10px; padding: 0 8px; background: #0066cc; color: white; border: none; border-radius: 4px;");
    connect(sendDrawBtn, &QPushButton::clicked, this, &SocoliteChat::onSendDrawing);
    tbLayout->addWidget(sendDrawBtn);

    drawLayout->addWidget(m_drawWidget);
    drawLayout->addWidget(toolbar);
    m_drawContainer->setVisible(false);
    layout->addWidget(m_drawContainer);

    // Input row
    auto *inputRow = new QWidget;
    auto *inputLayout = new QHBoxLayout(inputRow);
    inputLayout->setContentsMargins(0, 0, 0, 0);

    m_drawBtn = new QPushButton("Draw");
    m_drawBtn->setFixedHeight(36);
    m_drawBtn->setStyleSheet(
        "QPushButton { background: #333; color: #ccc; border: 1px solid #555; "
        "border-radius: 6px; padding: 6px 12px; font-size: 12px; }"
        "QPushButton:hover { background: #444; border-color: #0066cc; }");
    inputLayout->addWidget(m_drawBtn);

    m_input = new QLineEdit;
    m_input->setPlaceholderText("Type a message...");
    m_input->setFixedHeight(36);
    m_input->setStyleSheet(
        "QLineEdit { background: #2a2a2a; color: #eee; border: 1px solid #444; "
        "border-radius: 18px; padding: 4px 14px; font-size: 13px; }"
        "QLineEdit:focus { border-color: #0066cc; }");
    inputLayout->addWidget(m_input, 1);

    m_sendBtn = new QPushButton("Send");
    m_sendBtn->setFixedHeight(36);
    m_sendBtn->setStyleSheet(
        "QPushButton { background: #0066cc; color: white; border: none; "
        "border-radius: 18px; padding: 6px 18px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background: #0052a3; }");
    inputLayout->addWidget(m_sendBtn);

    layout->addWidget(inputRow);

    connect(m_sendBtn, &QPushButton::clicked, this, &SocoliteChat::onSend);
    connect(m_input, &QLineEdit::returnPressed, this, &SocoliteChat::onSend);
    connect(m_drawBtn, &QPushButton::clicked, this, [this]() {
        m_showingDraw = !m_showingDraw;
        m_drawContainer->setVisible(m_showingDraw);
        m_drawBtn->setText(m_showingDraw ? "Hide" : "Draw");
    });
}

void SocoliteChat::setFriend(const QString &friendId, const QString &friendName)
{
    m_friendId = friendId;
    m_friendName = friendName;
    // Clear messages
    QLayoutItem *child;
    while ((child = m_messageLayout->takeAt(0)) != nullptr) {
        if (child->widget()) delete child->widget();
        delete child;
    }
    m_messageLayout->addStretch();

    // Load history
    loadMessages(SocoliteEngine::instance().messages(friendId));
}

void SocoliteChat::addMessage(const SocoliteMessage &msg)
{
    auto *bubble = messageBubble(msg);
    m_messageLayout->insertWidget(m_messageLayout->count() - 1, bubble);
    QTimer::singleShot(50, this, [this]() {
        m_scrollArea->verticalScrollBar()->setValue(
            m_scrollArea->verticalScrollBar()->maximum());
    });
}

void SocoliteChat::loadMessages(const QVector<SocoliteMessage> &msgs)
{
    // Clear existing
    QLayoutItem *child;
    while ((child = m_messageLayout->takeAt(0)) != nullptr) {
        if (child->widget()) delete child->widget();
        delete child;
    }
    m_messageLayout->addStretch();

    for (const auto &msg : msgs)
        addMessage(msg);
}

void SocoliteChat::onSend()
{
    QString text = m_input->text().trimmed();
    if (text.isEmpty() || m_friendId.isEmpty()) return;
    m_input->clear();

    SocoliteMessage msg;
    msg.id = "local";
    msg.senderId = SocoliteEngine::instance().localUserId();
    msg.senderName = SocoliteEngine::instance().localUsername();
    msg.text = text;
    msg.timestamp = QDateTime::currentDateTime();
    msg.isLocal = true;

    addMessage(msg);
    emit sendMessage(m_friendId, text, {});
    SocoliteEngine::instance().sendMessage(m_friendId, text);
}

void SocoliteChat::onSendDrawing()
{
    if (m_friendId.isEmpty()) return;
    QImage img = m_drawWidget->canvasImage();
    if (img.isNull()) return;

    // Convert to PNG bytes
    QByteArray pngData;
    QBuffer buffer(&pngData);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");

    SocoliteMessage msg;
    msg.id = "local";
    msg.senderId = SocoliteEngine::instance().localUserId();
    msg.senderName = SocoliteEngine::instance().localUsername();
    msg.text = "Sent a drawing";
    msg.imageData = pngData;
    msg.timestamp = QDateTime::currentDateTime();
    msg.isLocal = true;

    addMessage(msg);
    SocoliteEngine::instance().sendMessage(m_friendId, {}, pngData);
    m_showingDraw = false;
    m_drawContainer->setVisible(false);
    m_drawBtn->setText("Draw");
}

QWidget *SocoliteChat::messageBubble(const SocoliteMessage &msg)
{
    auto *frame = new QFrame;
    bool local = msg.isLocal;
    frame->setStyleSheet(QString(
        "QFrame { background: %1; border-radius: 12px; padding: 10px; margin: 2px 0; }")
        .arg(local ? "#0066cc" : "#2a2a2a"));

    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(2);

    if (!local) {
        auto *name = new QLabel(msg.senderName);
        name->setStyleSheet("font-size: 10px; font-weight: bold; color: #0066cc;");
        layout->addWidget(name);
    }

    if (!msg.text.isEmpty()) {
        auto *text = new QLabel(msg.text);
        text->setStyleSheet(QString("font-size: 13px; color: %1;").arg(local ? "white" : "#eee"));
        text->setWordWrap(true);
        layout->addWidget(text);
    }

    if (!msg.imageData.isEmpty()) {
        QPixmap px;
        px.loadFromData(msg.imageData);
        if (!px.isNull()) {
            auto *img = new QLabel;
            img->setPixmap(px.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            img->setStyleSheet("border-radius: 8px;");
            layout->addWidget(img);
        }
    }

    auto *time = new QLabel(msg.timestamp.toString("hh:mm"));
    time->setStyleSheet("font-size: 9px; color: #888;");
    layout->addWidget(time);

    return frame;
}
