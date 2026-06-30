#pragma once

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
    static constexpr int kCellWidth  = 200;
    static constexpr int kCellHeight = 240;
    static constexpr int kArtHeight  = 180;
};
