#include "LibraryView.h"
#include "GameGridDelegate.h"
#include "UISoundManager.h"

#include <QVBoxLayout>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QEvent>
#include <QMouseEvent>

LibraryView::LibraryView(QWidget *parent, UISoundManager *soundMgr)
    : QWidget(parent)
    , m_model(new QStandardItemModel(this))
    , m_soundMgr(soundMgr)
{
    setupUI();
}

LibraryView::~LibraryView() = default;

void LibraryView::setupUI()
{
    auto *layout    = new QVBoxLayout(this);
    auto *searchBar = new QLineEdit(this);
    searchBar->setPlaceholderText("Search library...");

    auto *proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(m_model);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    m_view = new QListView(this);

    m_delegate = new GameGridDelegate(m_view);
    m_view->setItemDelegate(m_delegate);
    m_view->setModel(proxyModel);
    m_view->setViewMode(QListView::IconMode);
    m_view->setResizeMode(QListView::Adjust);
    m_view->setUniformItemSizes(true);
    m_view->viewport()->installEventFilter(this);

    layout->addWidget(searchBar);
    layout->addWidget(m_view);

    // Forward play button clicks from the delegate
    connect(m_delegate, &GameGridDelegate::playRequested,
            this, [this](const QString &romId) {
        emit playRequested(romId);
    });

    connect(searchBar, &QLineEdit::textChanged,
            proxyModel, &QSortFilterProxyModel::setFilterFixedString);
    connect(m_view, &QListView::clicked,
            this, &LibraryView::onItemClicked);
    connect(m_view, &QListView::doubleClicked,
            this, &LibraryView::onItemDoubleClicked);

    // Play move sound when keyboard selection changes
    connect(m_view->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this]() {
        if (m_soundMgr) m_soundMgr->play(UISoundManager::Move);
    });
}

bool LibraryView::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_view->viewport() && event->type() == QEvent::MouseMove) {
        auto *me = static_cast<QMouseEvent *>(event);
        QModelIndex idx = m_view->indexAt(me->pos());
        if (idx.isValid()) {
            QString romId = idx.data(Qt::UserRole).toString();
            if (romId != m_lastHoverId) {
                m_lastHoverId = romId;
                if (m_soundMgr) m_soundMgr->play(UISoundManager::Hover);
            }
        }
    }
    // Play select sound on click
    if (obj == m_view->viewport() && event->type() == QEvent::MouseButtonRelease) {
        if (m_soundMgr) m_soundMgr->play(UISoundManager::Select);
    }
    return QWidget::eventFilter(obj, event);
}

void LibraryView::setRoms(const QVector<RomInfo> &roms)
{
    m_model->clear();
    for (const auto &rom : roms) {
        auto *item = new QStandardItem(rom.title);
        item->setData(rom.id,       Qt::UserRole);
        item->setData(rom.filePath, Qt::UserRole + 1);
        item->setData(rom.platform, Qt::UserRole + 2);
        m_model->appendRow(item);
    }
}

void LibraryView::onItemClicked(const QModelIndex &index)
{
    const QString romId   = index.data(Qt::UserRole).toString();
    const QString romPath = index.data(Qt::UserRole + 1).toString();
    if (m_soundMgr) m_soundMgr->play(UISoundManager::Select);
    emit romSelected(romId, romPath);
}

void LibraryView::onItemDoubleClicked(const QModelIndex &index)
{
    const QString romId   = index.data(Qt::UserRole).toString();
    const QString romPath = index.data(Qt::UserRole + 1).toString();
    if (m_soundMgr) m_soundMgr->play(UISoundManager::Select);
    emit launchRequested(romId, romPath, "retroarch");
}
