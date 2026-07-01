#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QVector>
#include "SocoliteTypes.h"

class SocoliteUserSearch : public QWidget
{
    Q_OBJECT
public:
    explicit SocoliteUserSearch(QWidget *parent = nullptr);

signals:
    void addFriendRequested(const QString &userId);

private slots:
    void onSearch();
    void onResults(const QVector<SocoliteUser> &users);

private:
    void setupUI();
    QWidget *resultItem(const SocoliteUser &user);

    QLineEdit *m_searchInput = nullptr;
    QPushButton *m_searchBtn = nullptr;
    QListWidget *m_results = nullptr;
    QLabel *m_statusLabel = nullptr;
};
