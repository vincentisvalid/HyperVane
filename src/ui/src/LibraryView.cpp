#include "LibraryView.h"
#include "GameGridDelegate.h"

#include <QVBoxLayout>
#include <QLineEdit>
#include <QSortFilterProxyModel>

LibraryView::LibraryView(QWidget *parent)
    : QWidget(parent)
    , m_model(new QStandardItemModel(this))
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

    // Delegate is parented to m_view so Qt destroys it after the view,
    // preventing a use-after-free during teardown repaints.
    m_delegate = new GameGridDelegate(m_view);
    m_view->setItemDelegate(m_delegate);
    m_view->setModel(proxyModel);
    m_view->setViewMode(QListView::IconMode);
    m_view->setResizeMode(QListView::Adjust);
    m_view->setUniformItemSizes(true);

    layout->addWidget(searchBar);
    layout->addWidget(m_view);

    connect(searchBar, &QLineEdit::textChanged,
            proxyModel, &QSortFilterProxyModel::setFilterFixedString);
    connect(m_view, &QListView::clicked,
            this, &LibraryView::onItemClicked);
    connect(m_view, &QListView::doubleClicked,
            this, &LibraryView::onItemDoubleClicked);
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
    emit romSelected(romId, romPath);
}

void LibraryView::onItemDoubleClicked(const QModelIndex &index)
{
    const QString romId   = index.data(Qt::UserRole).toString();
    const QString romPath = index.data(Qt::UserRole + 1).toString();
    emit launchRequested(romId, romPath, "retroarch");
}
