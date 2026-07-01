#include "GameDetailDialog.h"
#include "GameArtLoader.h"
#include "ThemeEngine.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidgetItem>
#include <QPixmap>
#include <QScrollArea>
#include <QSplitter>
#include <QStyle>
#include <QVBoxLayout>

// ── Helpers ──────────────────────────────────────────────────────────────────

static QString formatSize(qint64 bytes)
{
    if (bytes < 1024) return QString::number(bytes) + " B";
    double kb = bytes / 1024.0;
    if (kb < 1024) return QString::number(kb, 'f', 1) + " KB";
    double mb = kb / 1024.0;
    if (mb < 1024) return QString::number(mb, 'f', 1) + " MB";
    double gb = mb / 1024.0;
    return QString::number(gb, 'f', 2) + " GB";
}

static QString buildDescription(const RomInfo &rom)
{
    if (!rom.description.isEmpty())
        return rom.description;
    // Generate a minimal description from metadata
    QStringList parts;
    parts << rom.title;
    if (!rom.platform.isEmpty())
        parts << "for" << rom.platform;
    if (!rom.region.isEmpty())
        parts << "(" + rom.region + ")";
    parts << "\n\nNo description available yet.";
    return parts.join(" ");
}

// ── Constructor ──────────────────────────────────────────────────────────────

GameDetailDialog::GameDetailDialog(const RomInfo &rom,
                                   const QString &previewPath,
                                   QWidget *parent)
    : QDialog(parent)
    , m_rom(rom)
    , m_previewPath(previewPath)
{
    setWindowTitle(rom.title + " — Details");
    setMinimumSize(800, 600);
    resize(960, 720);
    setAttribute(Qt::WA_DeleteOnClose, false);

    setupUI();
    loadCoverArt();
    populateScreenshotGallery();
    updateThemeStyles();
}

// ── UI Setup ─────────────────────────────────────────────────────────────────

void GameDetailDialog::setupUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Top section: cover + info + actions ────────────────────────────
    auto *topSection = new QWidget;
    auto *topLayout = new QHBoxLayout(topSection);
    topLayout->setContentsMargins(32, 24, 32, 24);
    topLayout->setSpacing(24);

    // Cover art
    m_coverArt = new QLabel;
    m_coverArt->setFixedSize(260, 340);
    m_coverArt->setAlignment(Qt::AlignCenter);
    m_coverArt->setStyleSheet(
        "background: #2a2a2a; border-radius: 12px; border: 1px solid #404040;");
    topLayout->addWidget(m_coverArt);

    // Info panel (right side)
    auto *infoLayout = new QVBoxLayout;
    infoLayout->setSpacing(8);

    m_titleLabel = new QLabel(m_rom.title);
    m_titleLabel->setStyleSheet("font-size: 28px; font-weight: bold; color: #eee;");
    m_titleLabel->setWordWrap(true);
    infoLayout->addWidget(m_titleLabel);

    QString meta = m_rom.platform;
    if (!m_rom.region.isEmpty())
        meta += "  |  " + m_rom.region;
    if (!m_rom.releaseDate.isEmpty())
        meta += "  |  " + m_rom.releaseDate;
    m_metaLabel = new QLabel(meta);
    m_metaLabel->setStyleSheet("font-size: 14px; color: #999;");
    infoLayout->addWidget(m_metaLabel);

    infoLayout->addSpacing(8);

    // ROM ID
    QString romIdStr = m_rom.romId.isEmpty() ? m_rom.id : m_rom.romId;
    m_romIdLabel = new QLabel("ROM ID: " + romIdStr);
    m_romIdLabel->setStyleSheet("font-size: 11px; color: #777; font-family: monospace;");
    infoLayout->addWidget(m_romIdLabel);

    // File size
    m_fileSizeLabel = new QLabel("Size: " + formatSize(m_rom.fileSize));
    m_fileSizeLabel->setStyleSheet("font-size: 11px; color: #777;");
    infoLayout->addWidget(m_fileSizeLabel);

    // Developer / Genre
    if (!m_rom.developer.isEmpty() || !m_rom.genre.isEmpty()) {
        QStringList extras;
        if (!m_rom.developer.isEmpty()) extras << m_rom.developer;
        if (!m_rom.genre.isEmpty()) extras << m_rom.genre;
        auto *extraLabel = new QLabel(extras.join("  ·  "));
        extraLabel->setStyleSheet("font-size: 12px; color: #aaa;");
        infoLayout->addWidget(extraLabel);
    }

    // Verified badge
    if (m_rom.verified) {
        auto *verifiedLabel = new QLabel("✓ Verified (No-Intro / Redump)");
        verifiedLabel->setStyleSheet("color: #00cc66; font-size: 11px; font-weight: 600;");
        infoLayout->addWidget(verifiedLabel);
    }

    infoLayout->addStretch();

    // Action buttons row
    auto *actionRow = new QHBoxLayout;
    actionRow->setSpacing(12);

    m_playBtn = new QPushButton("▶  Play");
    m_playBtn->setFixedHeight(44);
    m_playBtn->setCursor(Qt::PointingHandCursor);
    m_playBtn->setStyleSheet(
        "QPushButton { background: #0066cc; color: white; border: none; "
        "border-radius: 22px; padding: 0 32px; font-size: 15px; font-weight: 600; }"
        "QPushButton:hover { background: #0052a3; }");
    connect(m_playBtn, &QPushButton::clicked, this, [this]() {
        emit playRequested(m_rom.id);
    });
    actionRow->addWidget(m_playBtn);

    if (!m_previewPath.isEmpty()) {
        m_previewBtn = new QPushButton("▶  Watch Preview");
        m_previewBtn->setFixedHeight(44);
        m_previewBtn->setCursor(Qt::PointingHandCursor);
        m_previewBtn->setStyleSheet(
            "QPushButton { background: transparent; color: #00cc66; border: 2px solid #00cc66; "
            "border-radius: 22px; padding: 0 24px; font-size: 14px; font-weight: 600; }"
            "QPushButton:hover { background: rgba(0,204,102,0.1); }");
        connect(m_previewBtn, &QPushButton::clicked, this, [this]() {
            emit previewRequested(m_rom.id, m_rom.filePath);
        });
        actionRow->addWidget(m_previewBtn);
    }

    actionRow->addStretch();
    infoLayout->addLayout(actionRow);

    topLayout->addLayout(infoLayout, 1);

    // ── Description section ────────────────────────────────────────────
    auto *descSection = new QWidget;
    auto *descLayout = new QVBoxLayout(descSection);
    descLayout->setContentsMargins(32, 0, 32, 16);

    auto *descHeader = new QLabel("Description");
    descHeader->setStyleSheet("font-size: 16px; font-weight: 600; color: #ccc; padding-bottom: 6px;");
    descLayout->addWidget(descHeader);

    m_descriptionText = new QTextEdit;
    m_descriptionText->setReadOnly(true);
    m_descriptionText->setFrameShape(QFrame::NoFrame);
    m_descriptionText->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_descriptionText->setFixedHeight(100);
    m_descriptionText->setStyleSheet(
        "QTextEdit { background: transparent; color: #bbb; font-size: 13px; "
        "line-height: 1.5; border: none; }");
    m_descriptionText->setText(buildDescription(m_rom));
    descLayout->addWidget(m_descriptionText);

    // ── Screenshot gallery ─────────────────────────────────────────────
    auto *gallerySection = new QWidget;
    auto *galleryLayout = new QVBoxLayout(gallerySection);
    galleryLayout->setContentsMargins(32, 0, 32, 24);

    auto *galleryHeader = new QLabel("Screenshots & Gameplay");
    galleryHeader->setStyleSheet("font-size: 16px; font-weight: 600; color: #ccc; padding-bottom: 6px;");
    galleryLayout->addWidget(galleryHeader);

    m_screenshotGallery = new QListWidget;
    m_screenshotGallery->setViewMode(QListView::IconMode);
    m_screenshotGallery->setFlow(QListView::LeftToRight);
    m_screenshotGallery->setWrapping(false);
    m_screenshotGallery->setUniformItemSizes(true);
    m_screenshotGallery->setSpacing(8);
    m_screenshotGallery->setFixedHeight(180);
    m_screenshotGallery->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_screenshotGallery->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_screenshotGallery->setStyleSheet(
        "QListWidget { background: transparent; border: none; }"
        "QListWidget::item { background: #2a2a2a; border-radius: 8px; "
        "border: 1px solid #404040; }"
        "QListWidget::item:hover { border-color: #0066cc; }"
        "QListWidget::item:selected { border-color: #0066cc; background: #1a3a5a; }");
    connect(m_screenshotGallery, &QListWidget::itemClicked,
            this, [this](QListWidgetItem *item) {
        int idx = m_screenshotGallery->row(item);
        onScreenshotClicked(idx);
    });

    galleryLayout->addWidget(m_screenshotGallery);

    // ── Assemble the layout ────────────────────────────────────────────
    // Use a scroll area for the whole dialog
    auto *scrollContent = new QWidget;
    auto *scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setSpacing(0);
    scrollLayout->addWidget(topSection);
    scrollLayout->addWidget(descSection);
    scrollLayout->addWidget(gallerySection);
    scrollLayout->addStretch();

    auto *scrollArea = new QScrollArea;
    scrollArea->setWidget(scrollContent);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");

    root->addWidget(scrollArea, 1);
}

void GameDetailDialog::loadCoverArt()
{
    QPixmap art = GameArtLoader::instance().artFor(
        m_rom.id, m_rom.title, m_rom.platform);
    if (!art.isNull()) {
        m_coverArt->setPixmap(
            art.scaled(260, 340, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        // Request async load
        GameArtLoader::instance().prefetch(m_rom.id, m_rom.title, m_rom.platform);
        connect(&GameArtLoader::instance(), &GameArtLoader::artReady,
                this, [this](const QString &romId) {
            if (romId == m_rom.id) {
                QPixmap art2 = GameArtLoader::instance().artFor(
                    m_rom.id, m_rom.title, m_rom.platform);
                if (!art2.isNull())
                    m_coverArt->setPixmap(
                        art2.scaled(260, 340, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        });
        // Placeholder gradient
        m_coverArt->setText(m_rom.title.left(2).toUpper());
    }
}

void GameDetailDialog::populateScreenshotGallery()
{
    m_screenshotGallery->clear();

    // 1. Video preview thumbnail (if available)
    if (!m_previewPath.isEmpty() && QFile::exists(m_previewPath)) {
        auto *previewItem = new QListWidgetItem;
        auto *previewWidget = new QWidget;
        auto *pwLayout = new QVBoxLayout(previewWidget);
        pwLayout->setContentsMargins(4, 4, 4, 4);
        pwLayout->setSpacing(4);

        auto *thumbLabel = new QLabel;
        thumbLabel->setFixedSize(160, 120);
        thumbLabel->setAlignment(Qt::AlignCenter);
        thumbLabel->setStyleSheet(
            "background: #1a1a2e; border-radius: 6px; font-size: 32px; color: #0066cc;");
        thumbLabel->setText("\u25B6");
        pwLayout->addWidget(thumbLabel);

        auto *capLabel = new QLabel("Gameplay");
        capLabel->setAlignment(Qt::AlignCenter);
        capLabel->setStyleSheet("font-size: 10px; color: #888;");
        pwLayout->addWidget(capLabel);

        previewItem->setSizeHint(QSize(168, 148));
        m_screenshotGallery->addItem(previewItem);
        m_screenshotGallery->setItemWidget(previewItem, previewWidget);
    }

    // 2. Screenshots from RomInfo
    if (!m_rom.screenshots.isEmpty()) {
        for (const auto &screenshot : m_rom.screenshots) {
            auto *item = new QListWidgetItem;
            auto *sw = new QWidget;
            auto *sl = new QVBoxLayout(sw);
            sl->setContentsMargins(4, 4, 4, 4);
            sl->setSpacing(4);

            auto *imgLabel = new QLabel;
            imgLabel->setFixedSize(160, 120);
            imgLabel->setAlignment(Qt::AlignCenter);
            imgLabel->setStyleSheet("background: #1f1f1f; border-radius: 6px; font-size: 28px;");

            QPixmap ss(screenshot);
            if (!ss.isNull()) {
                imgLabel->setPixmap(
                    ss.scaled(160, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            } else {
                imgLabel->setText("+");
            }
            sl->addWidget(imgLabel);

            item->setSizeHint(QSize(168, 148));
            m_screenshotGallery->addItem(item);
            m_screenshotGallery->setItemWidget(item, sw);
        }
    }

    // 3. Placeholder slots (if we have room, show "coming soon" slots)
    int existing = m_screenshotGallery->count();
    int placeholders = qMax(0, 4 - existing);
    for (int i = 0; i < placeholders; ++i) {
        auto *item = new QListWidgetItem;
        auto *pw = new QWidget;
        auto *pl = new QVBoxLayout(pw);
        pl->setContentsMargins(4, 4, 4, 4);
        pl->setSpacing(4);

        auto *phLabel = new QLabel;
        phLabel->setFixedSize(160, 120);
        phLabel->setAlignment(Qt::AlignCenter);
        phLabel->setStyleSheet("background: #1f1f1f; border-radius: 6px; font-size: 16px; color: #555;");
        phLabel->setText("+");
        pl->addWidget(phLabel);

        auto *capLabel = new QLabel("Screenshot");
        capLabel->setAlignment(Qt::AlignCenter);
        capLabel->setStyleSheet("font-size: 10px; color: #555;");
        pl->addWidget(capLabel);

        item->setSizeHint(QSize(168, 148));
        m_screenshotGallery->addItem(item);
        m_screenshotGallery->setItemWidget(item, pw);
    }
}

void GameDetailDialog::onScreenshotClicked(int index)
{
    Q_UNUSED(index)
    // For now, clicking a screenshot triggers the preview
    if (!m_previewPath.isEmpty())
        emit previewRequested(m_rom.id, m_rom.filePath);
}

void GameDetailDialog::updateThemeStyles()
{
    // Apply theme from ThemeEngine to this dialog
    auto &te = ThemeEngine::instance();
    if (!te.toStyleSheet().isEmpty())
        setStyleSheet(te.toStyleSheet());
}
