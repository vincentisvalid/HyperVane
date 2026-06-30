#include "GameGridDelegate.h"

#include <QFile>
#include <QPainter>
#include <QStyleOptionViewItem>
#include <QModelIndex>
#include <QSvgRenderer>

static const QString kBadgeDir =
    QStringLiteral(HYPERVANE_ASSETS_DIR "/platform_badges/");

GameGridDelegate::GameGridDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{}

// Render the SVG badge for `platform` to a QPixmap and cache it.
// Falls back to "unknown" if the file doesn't exist.
const QPixmap &GameGridDelegate::badgePixmap(const QString &platform) const
{
    const QString key = platform.toLower();

    auto it = m_badgeCache.find(key);
    if (it != m_badgeCache.end())
        return it.value();

    auto tryLoad = [&](const QString &name) -> bool {
        const QString path = kBadgeDir + name + ".svg";
        if (!QFile::exists(path))
            return false;
        QSvgRenderer renderer(path);
        QPixmap pm(kBadgeW, kBadgeH);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        renderer.render(&p);
        m_badgeCache.insert(name, std::move(pm));
        return true;
    };

    if (!tryLoad(key))
        tryLoad(QStringLiteral("unknown"));

    // Insert an alias so the original key also hits the cache next time.
    if (!m_badgeCache.contains(key)) {
        m_badgeCache.insert(key, m_badgeCache.value(QStringLiteral("unknown")));
    }

    return m_badgeCache[key];
}

void GameGridDelegate::paint(QPainter *painter,
                              const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
{
    painter->save();

    const bool selected = option.state & QStyle::State_Selected;

    // Card background
    painter->fillRect(option.rect,
                      selected ? QColor(0x2a, 0x6e, 0xff, 180)
                               : QColor(0x1e, 0x1e, 0x2e));

    // Box-art area
    QRect artRect(option.rect.left() + 8,
                  option.rect.top()  + 8,
                  option.rect.width() - 16,
                  kArtHeight - 8);
    painter->fillRect(artRect, QColor(0x30, 0x30, 0x46));

    // Platform badge — bottom-right corner of art area, 4px inset
    const QString platform = index.data(Qt::UserRole + 2).toString();
    if (!platform.isEmpty()) {
        const QPixmap &badge = badgePixmap(platform);
        if (!badge.isNull()) {
            QPoint badgePos(artRect.right()  - kBadgeW - 4,
                            artRect.bottom() - kBadgeH - 4);
            painter->drawPixmap(badgePos, badge);
        }
    }

    // Title text below art
    QRect textRect(option.rect.left() + 4,
                   option.rect.top()  + kArtHeight + 4,
                   option.rect.width() - 8,
                   option.rect.height() - kArtHeight - 8);
    painter->setPen(Qt::white);
    painter->drawText(textRect,
                      Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                      index.data(Qt::DisplayRole).toString());

    painter->restore();
}

QSize GameGridDelegate::sizeHint(const QStyleOptionViewItem & /*option*/,
                                  const QModelIndex & /*index*/) const
{
    return {kCellWidth, kCellHeight};
}
