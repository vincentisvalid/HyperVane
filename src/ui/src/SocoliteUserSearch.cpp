#include "SocoliteUserSearch.h"
#include "SocoliteEngine.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>

SocoliteUserSearch::SocoliteUserSearch(QWidget *parent)
    : QWidget(parent)
{
    setupUI();

    connect(&SocoliteEngine::instance(), &SocoliteEngine::userSearchResults,
            this, &SocoliteUserSearch::onResults);
}

void SocoliteUserSearch::setupUI()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *header = new QLabel("Search Users");
    header->setStyleSheet("font-size: 16px; font-weight: bold; color: #0066cc; padding: 4px 0;");
    layout->addWidget(header);

    auto *searchRow = new QHBoxLayout;
    m_searchInput = new QLineEdit;
    m_searchInput->setPlaceholderText("Search by username...");
    m_searchInput->setStyleSheet(
        "QLineEdit { background: #2a2a2a; color: #eee; border: 1px solid #444; "
        "border-radius: 18px; padding: 6px 14px; font-size: 13px; }"
        "QLineEdit:focus { border-color: #0066cc; }");
    searchRow->addWidget(m_searchInput, 1);

    m_searchBtn = new QPushButton("Search");
    m_searchBtn->setStyleSheet(
        "QPushButton { background: #0066cc; color: white; border: none; "
        "border-radius: 18px; padding: 6px 16px; font-size: 13px; }"
        "QPushButton:hover { background: #0052a3; }");
    searchRow->addWidget(m_searchBtn);
    layout->addLayout(searchRow);

    m_statusLabel = new QLabel("Enter a username to search");
    m_statusLabel->setStyleSheet("font-size: 11px; color: #888; padding: 4px 0;");
    layout->addWidget(m_statusLabel);

    m_results = new QListWidget;
    m_results->setStyleSheet(
        "QListWidget { background: transparent; border: none; }"
        "QListWidget::item { padding: 2px; border: none; }");
    layout->addWidget(m_results, 1);

    connect(m_searchBtn, &QPushButton::clicked, this, &SocoliteUserSearch::onSearch);
    connect(m_searchInput, &QLineEdit::returnPressed, this, &SocoliteUserSearch::onSearch);
}

void SocoliteUserSearch::onSearch()
{
    QString query = m_searchInput->text().trimmed();
    if (query.isEmpty()) return;
    m_statusLabel->setText("Searching...");
    m_results->clear();
    SocoliteEngine::instance().searchUsers(query);
}

void SocoliteUserSearch::onResults(const QVector<SocoliteUser> &users)
{
    m_results->clear();
    if (users.isEmpty()) {
        m_statusLabel->setText("No users found");
        return;
    }
    m_statusLabel->setText(QString("Found %1 user(s)").arg(users.size()));

    for (const auto &u : users) {
        auto *widget = resultItem(u);
        auto *item = new QListWidgetItem;
        item->setData(Qt::UserRole, u.id);
        item->setSizeHint(widget->sizeHint());
        m_results->addItem(item);
        m_results->setItemWidget(item, widget);
    }
}

QWidget *SocoliteUserSearch::resultItem(const SocoliteUser &user)
{
    auto *frame = new QFrame;
    frame->setStyleSheet("QFrame { background: rgba(255,255,255,0.05); border-radius: 8px; padding: 8px; }");

    auto *layout = new QHBoxLayout(frame);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(10);

    // Avatar
    auto *avatar = new QLabel;
    avatar->setFixedSize(40, 40);
    avatar->setStyleSheet("background: #0066cc; border-radius: 20px;");
    layout->addWidget(avatar);

    // Info
    auto *infoLayout = new QVBoxLayout;
    infoLayout->setSpacing(0);
    auto *name = new QLabel(user.displayName.isEmpty() ? user.username : user.displayName);
    name->setStyleSheet("font-size: 13px; font-weight: bold; color: #eee;");
    infoLayout->addWidget(name);

    auto *status = new QLabel(QString("@%1  ·  %2").arg(user.username, user.status));
    status->setStyleSheet("font-size: 10px; color: #888;");
    infoLayout->addWidget(status);
    layout->addLayout(infoLayout, 1);

    if (!user.isFriend) {
        auto *addBtn = new QPushButton("+ Add");
        addBtn->setFixedHeight(28);
        addBtn->setStyleSheet(
            "QPushButton { background: transparent; color: #0066cc; border: 1px solid #0066cc; "
            "border-radius: 14px; padding: 0 12px; font-size: 11px; }"
            "QPushButton:hover { background: #0066cc; color: white; }");
        connect(addBtn, &QPushButton::clicked, this, [this, id = user.id]() {
            SocoliteEngine::instance().addFriend(id);
            emit addFriendRequested(id);
        });
        layout->addWidget(addBtn);
    }

    return frame;
}
