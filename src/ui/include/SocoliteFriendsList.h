#pragma once

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QVector>
#include "SocoliteTypes.h"

class SocoliteFriendsList : public QWidget
{
    Q_OBJECT
public:
    explicit SocoliteFriendsList(QWidget *parent = nullptr);

    void refresh();

signals:
    void friendSelected(const QString &friendId);
    void addFriendClicked();

private:
    void setupUI();
    QWidget *friendItem(const SocoliteUser &user);

    QListWidget *m_list = nullptr;
    QPushButton *m_addBtn = nullptr;
    QLabel *m_onlineCount = nullptr;
};
