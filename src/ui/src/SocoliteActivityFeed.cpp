#include "SocoliteActivityFeed.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QDateTime>

SocoliteActivityFeed::SocoliteActivityFeed(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void SocoliteActivityFeed::setupUI()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *header = new QLabel("Activity Feed");
    header->setStyleSheet("font-size: 16px; font-weight: bold; color: #0066cc; padding: 4px 0;");
    layout->addWidget(header);

    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");

    m_container = new QWidget;
    m_layout = new QVBoxLayout(m_container);
    m_layout->setAlignment(Qt::AlignTop);
    m_layout->setSpacing(6);
    m_scrollArea->setWidget(m_container);
    layout->addWidget(m_scrollArea, 1);

    auto *placeholder = new QLabel("No recent activity");
    placeholder->setStyleSheet("color: #666; font-size: 13px; padding: 20px;");
    placeholder->setAlignment(Qt::AlignCenter);
    m_layout->addWidget(placeholder);
}

void SocoliteActivityFeed::addActivity(const SocoliteActivity &activity)
{
    // Remove placeholder if present
    if (m_layout->count() == 1) {
        auto *first = m_layout->itemAt(0);
        if (first && first->widget()) {
            QString text = qobject_cast<QLabel *>(first->widget())->text();
            if (text == "No recent activity") {
                delete first->widget();
                m_layout->removeItem(first);
            }
        }
    }

    auto *card = activityCard(activity);
    m_layout->insertWidget(0, card);

    // Limit visible activities
    while (m_layout->count() > 50) {
        auto *last = m_layout->itemAt(m_layout->count() - 1);
        if (last && last->widget()) {
            delete last->widget();
            m_layout->removeItem(last);
        }
    }
}

void SocoliteActivityFeed::loadActivities(const QVector<SocoliteActivity> &activities)
{
    // Clear existing
    QLayoutItem *child;
    while ((child = m_layout->takeAt(0)) != nullptr) {
        if (child->widget()) delete child->widget();
        delete child;
    }

    if (activities.isEmpty()) {
        auto *placeholder = new QLabel("No recent activity");
        placeholder->setStyleSheet("color: #666; font-size: 13px; padding: 20px;");
        placeholder->setAlignment(Qt::AlignCenter);
        m_layout->addWidget(placeholder);
        return;
    }

    for (const auto &act : activities)
        m_layout->addWidget(activityCard(act));
}

QWidget *SocoliteActivityFeed::activityCard(const SocoliteActivity &activity)
{
    auto *frame = new QFrame;
    frame->setStyleSheet("QFrame { background: rgba(255,255,255,0.05); border-radius: 10px; padding: 10px; margin: 2px 0; }");

    auto *layout = new QHBoxLayout(frame);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(10);

    // Icon
    auto *icon = new QLabel;
    icon->setFixedSize(36, 36);
    QString iconStyle;
    switch (activity.type) {
    case SocoliteActivity::GameStarted: iconStyle = "background: #00cc66; border-radius: 18px;"; break;
    case SocoliteActivity::ScreenshotShared: iconStyle = "background: #0066cc; border-radius: 18px;"; break;
    case SocoliteActivity::FriendAdded: iconStyle = "background: #9b59b6; border-radius: 18px;"; break;
    case SocoliteActivity::ArtworkShared: iconStyle = "background: #e5a00d; border-radius: 18px;"; break;
    case SocoliteActivity::Achievement: iconStyle = "background: #e50914; border-radius: 18px;"; break;
    default: iconStyle = "background: #555; border-radius: 18px;"; break;
    }
    icon->setStyleSheet(iconStyle);
    layout->addWidget(icon);

    // Content
    auto *contentLayout = new QVBoxLayout;
    contentLayout->setSpacing(2);

    auto *msg = new QLabel(activity.message);
    msg->setStyleSheet("font-size: 12px; color: #eee; font-weight: 500;");
    msg->setWordWrap(true);
    contentLayout->addWidget(msg);

    if (!activity.detail.isEmpty()) {
        auto *detail = new QLabel(activity.detail);
        detail->setStyleSheet("font-size: 11px; color: #888;");
        contentLayout->addWidget(detail);
    }

    auto *time = new QLabel(activity.timestamp.toString("MMM d, hh:mm"));
    time->setStyleSheet("font-size: 10px; color: #666;");
    contentLayout->addWidget(time);
    layout->addLayout(contentLayout, 1);

    // Optional image thumbnail
    if (!activity.imageData.isEmpty()) {
        QPixmap px;
        px.loadFromData(activity.imageData);
        if (!px.isNull()) {
            auto *img = new QLabel;
            img->setPixmap(px.scaled(60, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            img->setStyleSheet("border-radius: 6px;");
            layout->addWidget(img);
        }
    }

    return frame;
}
