#include "GameGridDelegate.h"

#include <QPainter>
#include <QStyleOptionViewItem>
#include <QModelIndex>
#include <QPixmapCache>

GameGridDelegate::GameGridDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{}

void GameGridDelegate::paint(QPainter *painter,
                              const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
{
    painter->save();

    const bool selected = option.state & QStyle::State_Selected;

    // Background
    painter->fillRect(option.rect,
                      selected ? QColor(0x2a, 0x6e, 0xff, 180)
                               : QColor(0x1e, 0x1e, 0x2e));

    // Box-art placeholder (replaced with actual art once asset pipeline is wired)
    QRect artRect(option.rect.left() + 8,
                  option.rect.top()  + 8,
                  option.rect.width() - 16,
                  kArtHeight - 8);
    painter->fillRect(artRect, QColor(0x30, 0x30, 0x46));

    // Title — always placed at a fixed offset below the art area
    QString title = index.data(Qt::DisplayRole).toString();
    QRect textRect(option.rect.left() + 4,
                   option.rect.top()  + kArtHeight + 4,
                   option.rect.width() - 8,
                   option.rect.height() - kArtHeight - 8);
    painter->setPen(Qt::white);
    painter->drawText(textRect,
                      Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                      title);

    painter->restore();
}

QSize GameGridDelegate::sizeHint(const QStyleOptionViewItem & /*option*/,
                                  const QModelIndex & /*index*/) const
{
    return {kCellWidth, kCellHeight};
}
