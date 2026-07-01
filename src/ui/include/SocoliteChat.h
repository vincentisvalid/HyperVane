#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QVector>
#include "SocoliteTypes.h"

class SocoliteCanvas;

class SocoliteChat : public QWidget
{
    Q_OBJECT
public:
    explicit SocoliteChat(QWidget *parent = nullptr);

    void setFriend(const QString &friendId, const QString &friendName);
    void addMessage(const SocoliteMessage &msg);
    void loadMessages(const QVector<SocoliteMessage> &msgs);

signals:
    void sendMessage(const QString &friendId, const QString &text, const QByteArray &imageData);
    void sendDrawing(const QString &friendId, const QImage &image);

private:
    void setupUI();
    void onSend();
    void onSendDrawing();
    QWidget *messageBubble(const SocoliteMessage &msg);

    QString m_friendId;
    QString m_friendName;
    QLineEdit *m_input = nullptr;
    QPushButton *m_sendBtn = nullptr;
    QPushButton *m_drawBtn = nullptr;
    QWidget *m_messageArea = nullptr;
    QVBoxLayout *m_messageLayout = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    SocoliteCanvas *m_drawWidget = nullptr;
    QWidget *m_drawContainer = nullptr;
    bool m_showingDraw = false;
};
