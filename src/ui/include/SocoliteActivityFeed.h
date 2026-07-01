#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QLabel>
#include <QVector>
#include "SocoliteTypes.h"

class SocoliteActivityFeed : public QWidget
{
    Q_OBJECT
public:
    explicit SocoliteActivityFeed(QWidget *parent = nullptr);

    void addActivity(const SocoliteActivity &activity);
    void loadActivities(const QVector<SocoliteActivity> &activities);

private:
    void setupUI();
    QWidget *activityCard(const SocoliteActivity &activity);

    QWidget *m_container = nullptr;
    QVBoxLayout *m_layout = nullptr;
    QScrollArea *m_scrollArea = nullptr;
};
