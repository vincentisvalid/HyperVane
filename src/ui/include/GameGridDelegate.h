#pragma once

#include <QHash>
#include <QPixmap>
#include <QStyledItemDelegate>

class GameGridDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit GameGridDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

private:
    const QPixmap &badgePixmap(const QString &platform) const;

    static constexpr int kCellWidth  = 200;
    static constexpr int kCellHeight = 240;
    static constexpr int kArtHeight  = 180;
    static constexpr int kBadgeW     = 64;
    static constexpr int kBadgeH     = 22;

    // Lazily populated on first paint — keyed by lowercase platform string.
    mutable QHash<QString, QPixmap> m_badgeCache;
};
