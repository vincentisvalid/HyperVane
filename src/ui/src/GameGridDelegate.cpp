#include "GameGridDelegate.h"
#include "GameArtLoader.h"

#include <QApplication>
#include <QEvent>
#include <QFile>
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStyleOptionViewItem>
#include <QModelIndex>
#include <QSvgRenderer>

static const QString kBadgeDir =
    QStringLiteral(HYPERVANE_ASSETS_DIR "/platform_badges/");

GameGridDelegate::GameGridDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{}

void GameGridDelegate::setDarkMode(bool dark)
{
    if (dark) {
        m_cardBgNormal        = QColor(0x2a, 0x2a, 0x2a);
        m_cardBgSelected      = QColor(0x1a, 0x3a, 0x6a);
        m_textColor           = QColor(0xff, 0xff, 0xff);
        m_mutedTextColor      = QColor(0x99, 0x99, 0x99);
        m_artPlaceholderStart = QColor(0x33, 0x33, 0x33);
        m_artPlaceholderEnd   = QColor(0x2a, 0x2a, 0x2a);
        m_detailsBtnBg        = QColor(0x55, 0x55, 0x55);
        m_detailsBtnHoverBg   = QColor(0x00, 0x66, 0xcc);
    } else {
        m_cardBgNormal        = QColor(0xf5, 0xf5, 0xf5);
        m_cardBgSelected      = QColor(0xe8, 0xf0, 0xfe);
        m_textColor           = QColor(0x00, 0x00, 0x00);
        m_mutedTextColor      = QColor(0x66, 0x66, 0x66);
        m_artPlaceholderStart = QColor(0xe0, 0xe0, 0xe0);
        m_artPlaceholderEnd   = QColor(0xcc, 0xcc, 0xcc);
        m_detailsBtnBg        = QColor(0x88, 0x88, 0x88);
        m_detailsBtnHoverBg   = QColor(0x00, 0x66, 0xcc);
    }
}

const QPixmap &GameGridDelegate::badgePixmap(const QString &platform) const
{
    const QString key = platform.toLower();
    auto it = m_badgeCache.find(key);
    if (it != m_badgeCache.end())
        return it.value();

    auto tryLoad = [&](const QString &name) -> bool {
        const QString path = kBadgeDir + name + ".svg";
        if (!QFile::exists(path)) return false;
        QSvgRenderer renderer(path);
        QPixmap pm(kBadgeW, kBadgeH);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        renderer.render(&p);
        m_badgeCache.insert(name, std::move(pm));
        return true;
    };

    if (!tryLoad(key))
        tryLoad(QStringLiteral("unknown"));

    if (!m_badgeCache.contains(key))
        m_badgeCache.insert(key, m_badgeCache.value(QStringLiteral("unknown")));

    return m_badgeCache[key];
}

static QPainterPath squirclePath(const QRectF &r, qreal radius)
{
    QPainterPath p;
    p.addRoundedRect(r, radius, radius);
    return p;
}

QRect GameGridDelegate::playButtonRect(const QStyleOptionViewItem &option) const
{
    const QRectF card(option.rect.left()  + kCardMargin,
                       option.rect.top()   + kCardMargin,
                       option.rect.width() - kCardMargin * 2,
                       option.rect.height()- kCardMargin * 2);
    int bw = 48, bh = 48;
    return QRect(static_cast<int>(card.right()) - bw - 10,
                 static_cast<int>(card.top()) + kArtHeight - bh - 10,
                 bw, bh);
}

QRect GameGridDelegate::detailButtonRect(const QStyleOptionViewItem &option) const
{
    const QRectF card(option.rect.left()  + kCardMargin,
                       option.rect.top()   + kCardMargin,
                       option.rect.width() - kCardMargin * 2,
                       option.rect.height()- kCardMargin * 2);
    int d = 22;
    return QRect(static_cast<int>(card.left()) + 8,
                 static_cast<int>(card.top()) + 8,
                 d, d);
}

void GameGridDelegate::paint(QPainter *painter,
                              const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
{
    painter->save();
    painter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing
                            | QPainter::SmoothPixmapTransform);

    const bool hovered  = option.state & QStyle::State_MouseOver;
    const bool selected = option.state & QStyle::State_Selected;

    const QRectF card(option.rect.left()  + kCardMargin,
                       option.rect.top()   + kCardMargin,
                       option.rect.width() - kCardMargin * 2,
                       option.rect.height()- kCardMargin * 2);

    const QString romId   = index.data(Qt::UserRole).toString();
    const QString title   = index.data(Qt::DisplayRole).toString();
    const QString platform = index.data(Qt::UserRole + 2).toString();

    // ── Card shadow ──────────────────────────────────────────────────────
    if (hovered || selected) {
        const QColor glowColor = selected ? QColor(0x00, 0x66, 0xcc, 80)
                                           : QColor(0x00, 0x66, 0xcc, 40);
        for (int i = 3; i >= 1; --i) {
            QRectF shadowRect = card.adjusted(-i*2, -i*2, i*2, i*2);
            QPainterPath shadow = squirclePath(shadowRect, kRadius + i*2);
            painter->fillPath(shadow, glowColor);
        }
    }

    // ── Card background ──────────────────────────────────────────────────
    const QPainterPath cardPath = squirclePath(card, kRadius);
    QColor cardBg = selected ? m_cardBgSelected : m_cardBgNormal;
    painter->fillPath(cardPath, cardBg);

    // ── Art area ─────────────────────────────────────────────────────────
    const QRectF artRect(card.left(), card.top(), card.width(), kArtHeight);
    const QPainterPath artPath = squirclePath(artRect, kRadius);
    painter->setClipPath(artPath & cardPath);

    // Try to load box art
    QPixmap art = GameArtLoader::instance().artFor(romId, title, platform);
    if (!art.isNull()) {
        QRectF artDrawRect = artRect;
        QPixmap scaled = art.scaled(static_cast<int>(artRect.width()),
                                    static_cast<int>(artRect.height()),
                                    Qt::KeepAspectRatioByExpanding,
                                    Qt::SmoothTransformation);
        int ox = static_cast<int>((scaled.width() - artRect.width()) / 2);
        int oy = static_cast<int>((scaled.height() - artRect.height()) / 2);
        painter->drawPixmap(QPointF(artRect.left(), artRect.top()),
                            scaled, QRectF(ox, oy, artRect.width(), artRect.height()));
    } else {
        QLinearGradient artGrad(artRect.topLeft(), artRect.bottomLeft());
        artGrad.setColorAt(0.0, m_artPlaceholderStart);
        artGrad.setColorAt(1.0, m_artPlaceholderEnd);
        painter->fillRect(artRect, artGrad);

        QLinearGradient shimmer(artRect.topLeft(), artRect.bottomRight());
        shimmer.setColorAt(0.0, QColor(0x00, 0x66, 0xcc, 12));
        shimmer.setColorAt(1.0, Qt::transparent);
        painter->fillRect(artRect, shimmer);
    }

    painter->setClipping(false);

    // ── Play button on hover ────────────────────────────────────────────
    if (hovered) {
        QRectF btnRect = QRectF(playButtonRect(option));
        QPainterPath btnPath;
        btnPath.addEllipse(btnRect);
        painter->fillPath(btnPath, QColor(0x00, 0x66, 0xcc, 220));

        QFont iconFont = painter->font();
        iconFont.setPointSize(16);
        iconFont.setBold(true);
        painter->setFont(iconFont);
        painter->setPen(Qt::white);
        QRectF textRect = btnRect.adjusted(2, 2, -2, -2);
        painter->drawText(textRect, Qt::AlignCenter, QString::fromUtf8("\u25B6"));

        // ── Detail (i) button on hover ──────────────────────────────────
        QRectF dtlRect = QRectF(detailButtonRect(option));
        QPainterPath dtlPath;
        dtlPath.addEllipse(dtlRect);
        painter->fillPath(dtlPath, m_detailsBtnBg);

        QFont dtlFont = painter->font();
        dtlFont.setPointSize(9);
        dtlFont.setBold(true);
        painter->setFont(dtlFont);
        painter->setPen(Qt::white);
        painter->drawText(dtlRect.adjusted(1, 1, -1, -1), Qt::AlignCenter, QStringLiteral("i"));
    }

    // ── Platform badge ───────────────────────────────────────────────────
    if (!platform.isEmpty()) {
        const QPixmap &badge = badgePixmap(platform);
        if (!badge.isNull()) {
            QPointF badgePos(card.right()  - kBadgeW - 6,
                             card.top()    + kArtHeight - kBadgeH - 6);
            painter->drawPixmap(badgePos.toPoint(), badge);
        }
    }

    // ── Title ─────────────────────────────────────────────────────────────
    const QRectF textArea(card.left() + 10,
                          card.top()  + kArtHeight + 8,
                          card.width() - 20,
                          card.height() - kArtHeight - 10);

    QFont titleFont = painter->font();
    titleFont.setPointSizeF(10.5);
    titleFont.setWeight(QFont::DemiBold);
    painter->setFont(titleFont);
    painter->setPen(m_textColor);
    painter->drawText(textArea,
                      Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                      title);

    // ── Platform text ─────────────────────────────────────────────────────
    QFont platFont = painter->font();
    platFont.setPointSizeF(8);
    platFont.setWeight(QFont::Normal);
    painter->setFont(platFont);
    painter->setPen(m_mutedTextColor);
    QRectF platArea = textArea;
    platArea.setTop(textArea.bottom() - 20);
    painter->drawText(platArea, Qt::AlignLeft | Qt::AlignBottom, platform);

    // ── Selected border ───────────────────────────────────────────────────
    if (selected) {
        painter->setPen(QPen(QColor(0x00, 0x66, 0xcc), 2));
        painter->drawPath(cardPath);
    }

    painter->restore();
}

bool GameGridDelegate::editorEvent(QEvent *event,
                                    QAbstractItemModel *model,
                                    const QStyleOptionViewItem &option,
                                    const QModelIndex &index)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (playButtonRect(option).contains(me->pos())) {
            QString romId = index.data(Qt::UserRole).toString();
            if (!romId.isEmpty())
                emit playRequested(romId);
            return true;
        }
        if (detailButtonRect(option).contains(me->pos())) {
            QString romId = index.data(Qt::UserRole).toString();
            if (!romId.isEmpty())
                emit detailsRequested(romId);
            return true;
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

QSize GameGridDelegate::sizeHint(const QStyleOptionViewItem &,
                                  const QModelIndex &) const
{
    return {kCellWidth, kCellHeight};
}
