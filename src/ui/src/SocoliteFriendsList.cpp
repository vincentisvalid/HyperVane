#include "SocoliteFriendsList.h"
#include "SocoliteEngine.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>

SocoliteFriendsList::SocoliteFriendsList(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    connect(&SocoliteEngine::instance(), &SocoliteEngine::friendsListChanged,
            this, &SocoliteFriendsList::refresh);
}

void SocoliteFriendsList::setupUI()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *header = new QLabel("Socolite Friends");
    header->setStyleSheet("font-size: 16px; font-weight: bold; color: #0066cc; padding: 4px 0;");
    layout->addWidget(header);

    m_onlineCount = new QLabel("0 online");
    m_onlineCount->setStyleSheet("font-size: 11px; color: #888; padding-bottom: 4px;");
    layout->addWidget(m_onlineCount);

    m_list = new QListWidget;
    m_list->setStyleSheet(
        "QListWidget { background: transparent; border: none; }"
        "QListWidget::item { padding: 2px; border: none; }");
    m_list->setSpacing(2);
    layout->addWidget(m_list, 1);

    m_addBtn = new QPushButton("+ Add Friend");
    m_addBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #0066cc; border: 1px dashed #0066cc; "
        "border-radius: 6px; padding: 8px; font-size: 12px; }"
        "QPushButton:hover { background: rgba(0,102,204,0.1); }");
    layout->addWidget(m_addBtn);

    connect(m_addBtn, &QPushButton::clicked, this, &SocoliteFriendsList::addFriendClicked);
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        auto *item = m_list->item(row);
        if (item)
            emit friendSelected(item->data(Qt::UserRole).toString());
    });

    refresh();
}

void SocoliteFriendsList::refresh()
{
    m_list->clear();
    auto &engine = SocoliteEngine::instance();
    auto friends = engine.friends();

    int online = 0;
    for (const auto &f : friends) {
        if (f.status != "offline") online++;
        auto *widget = friendItem(f);
        auto *item = new QListWidgetItem;
        item->setData(Qt::UserRole, f.id);
        item->setSizeHint(widget->sizeHint());
        m_list->addItem(item);
        m_list->setItemWidget(item, widget);
    }
    m_onlineCount->setText(QString("%1 online  ·  %2 total").arg(online).arg(friends.size()));
}

QWidget *SocoliteFriendsList::friendItem(const SocoliteUser &user)
{
    auto *frame = new QFrame;
    frame->setStyleSheet("QFrame { background: rgba(255,255,255,0.05); border-radius: 8px; padding: 6px; }");

    auto *layout = new QHBoxLayout(frame);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(8);

    // Status dot
    auto *dot = new QLabel;
    dot->setFixedSize(10, 10);
    QString dotColor;
    if (user.status == "online") dotColor = "#00cc66";
    else if (user.status == "ingame") dotColor = "#0066cc";
    else if (user.status == "away") dotColor = "#e5a00d";
    else if (user.status == "busy") dotColor = "#e50914";
    else dotColor = "#555";
    dot->setStyleSheet(QString("background: %1; border-radius: 5px;").arg(dotColor));
    layout->addWidget(dot);

    // Avatar thumbnail
    auto *avatar = new QLabel;
    avatar->setFixedSize(32, 32);
    if (!user.avatarPath.isEmpty()) {
        QPixmap px(user.avatarPath);
        avatar->setPixmap(px.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        avatar->setStyleSheet("background: #0066cc; border-radius: 16px;");
    }
    layout->addWidget(avatar);

    // Name + status
    auto *infoLayout = new QVBoxLayout;
    infoLayout->setSpacing(0);
    auto *name = new QLabel(user.displayName.isEmpty() ? user.username : user.displayName);
    name->setStyleSheet("font-size: 12px; font-weight: bold; color: #eee;");
    infoLayout->addWidget(name);

    QString statusText;
    if (user.status == "ingame" && !user.gameTitle.isEmpty())
        statusText = "Playing " + user.gameTitle;
    else
        statusText = user.status;
    auto *statusLabel = new QLabel(statusText);
    statusLabel->setStyleSheet("font-size: 10px; color: #888;");
    infoLayout->addWidget(statusLabel);
    layout->addLayout(infoLayout, 1);

    return frame;
}
