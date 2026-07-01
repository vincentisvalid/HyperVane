#pragma once

#include <QColor>
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

    bool editorEvent(QEvent *event,
                     QAbstractItemModel *model,
                     const QStyleOptionViewItem &option,
                     const QModelIndex &index) override;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

    QRect playButtonRect(const QStyleOptionViewItem &option) const;
    QRect detailButtonRect(const QStyleOptionViewItem &option) const;

    void setDarkMode(bool dark);

signals:
    void playRequested(const QString &romId);
    void detailsRequested(const QString &romId);

private:
    const QPixmap &badgePixmap(const QString &platform) const;

    static constexpr int kCellWidth  = 210;
    static constexpr int kCellHeight = 310;
    static constexpr int kArtHeight  = 235;
    static constexpr int kBadgeW     = 64;
    static constexpr int kBadgeH     = 22;
    static constexpr int kCardMargin = 6;
    static constexpr int kRadius     = 16;

    mutable QHash<QString, QPixmap> m_badgeCache;

    // Theme-aware colors
    QColor m_cardBgNormal       = QColor(0xf5, 0xf5, 0xf5);
    QColor m_cardBgSelected     = QColor(0xe8, 0xf0, 0xfe);
    QColor m_textColor          = QColor(0x00, 0x00, 0x00);
    QColor m_mutedTextColor     = QColor(0x66, 0x66, 0x66);
    QColor m_artPlaceholderStart = QColor(0xe0, 0xe0, 0xe0);
    QColor m_artPlaceholderEnd  = QColor(0xcc, 0xcc, 0xcc);
    QColor m_detailsBtnBg       = QColor(0x88, 0x88, 0x88);
    QColor m_detailsBtnHoverBg  = QColor(0x00, 0x66, 0xcc);
};
