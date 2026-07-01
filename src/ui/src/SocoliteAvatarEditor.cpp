#include "SocoliteAvatarEditor.h"
#include "SocoliteEngine.h"

#include <QBuffer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPainter>
#include <QColorDialog>
#include <QGroupBox>
#include <QtMath>

SocoliteAvatarEditor::SocoliteAvatarEditor(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    regeneratePreview();
}

void SocoliteAvatarEditor::setupUI()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *header = new QLabel("Socolite Avatar Editor");
    header->setStyleSheet("font-size: 16px; font-weight: bold; color: #0066cc;");
    layout->addWidget(header);
    layout->addSpacing(8);

    // Preview
    auto *previewGroup = new QGroupBox("Preview");
    auto *previewLayout = new QHBoxLayout(previewGroup);
    m_preview = new QLabel;
    m_preview->setFixedSize(128, 128);
    m_preview->setStyleSheet("background: #1f1f1f; border: 2px solid #444; border-radius: 64px;");
    previewLayout->addWidget(m_preview, 0, Qt::AlignCenter);
    layout->addWidget(previewGroup);

    // Shapes
    auto *shapeGroup = new QGroupBox("Shapes");
    auto *shapeLayout = new QGridLayout(shapeGroup);
    shapeLayout->addWidget(new QLabel("Background:"), 0, 0);
    m_bgShape = new QComboBox;
    m_bgShape->addItems({"Circle", "Square", "Triangle"});
    shapeLayout->addWidget(m_bgShape, 0, 1);

    shapeLayout->addWidget(new QLabel("Foreground:"), 1, 0);
    m_fgShape = new QComboBox;
    m_fgShape->addItems({"Circle", "Square", "Triangle", "Star"});
    shapeLayout->addWidget(m_fgShape, 1, 1);
    layout->addWidget(shapeGroup);

    // Colors
    auto *colorGroup = new QGroupBox("Colors");
    auto *colorLayout = new QGridLayout(colorGroup);
    colorLayout->addWidget(new QLabel("Background:"), 0, 0);
    m_bgColorBtn = new QPushButton;
    m_bgColorBtn->setFixedSize(40, 24);
    m_bgColorBtn->setStyleSheet(QString("background: %1; border: 2px solid #555; border-radius: 4px;").arg(m_bgColor.name()));
    colorLayout->addWidget(m_bgColorBtn, 0, 1);

    colorLayout->addWidget(new QLabel("Foreground:"), 1, 0);
    m_fgColorBtn = new QPushButton;
    m_fgColorBtn->setFixedSize(40, 24);
    m_fgColorBtn->setStyleSheet(QString("background: %1; border: 2px solid #555; border-radius: 4px;").arg(m_fgColor.name()));
    colorLayout->addWidget(m_fgColorBtn, 1, 1);
    layout->addWidget(colorGroup);

    // Size
    auto *sizeGroup = new QGroupBox("Size");
    auto *sizeLayout = new QHBoxLayout(sizeGroup);
    sizeLayout->addWidget(new QLabel("Avatar Size:"));
    m_sizeSlider = new QSlider(Qt::Horizontal);
    m_sizeSlider->setRange(32, 256);
    m_sizeSlider->setValue(m_avatarSize);
    sizeLayout->addWidget(m_sizeSlider, 1);
    auto *sizeLabel = new QLabel(QString::number(m_avatarSize));
    sizeLayout->addWidget(sizeLabel);
    layout->addWidget(sizeGroup);

    connect(m_sizeSlider, &QSlider::valueChanged, this, [this, sizeLabel](int v) {
        m_avatarSize = v;
        sizeLabel->setText(QString::number(v));
        regeneratePreview();
    });

    // Buttons
    auto *btnLayout = new QHBoxLayout;
    auto *acceptBtn = new QPushButton("Set Avatar");
    acceptBtn->setStyleSheet("background: #0066cc; color: white; border: none; border-radius: 6px; padding: 10px 20px; font-weight: bold;");
    btnLayout->addStretch();
    btnLayout->addWidget(acceptBtn);
    layout->addLayout(btnLayout);

    connect(m_bgShape, &QComboBox::currentTextChanged, this, [this]() { regeneratePreview(); });
    connect(m_fgShape, &QComboBox::currentTextChanged, this, [this]() { regeneratePreview(); });
    connect(m_bgColorBtn, &QPushButton::clicked, this, &SocoliteAvatarEditor::onColorChanged);
    connect(m_fgColorBtn, &QPushButton::clicked, this, &SocoliteAvatarEditor::onColorChanged);
    connect(acceptBtn, &QPushButton::clicked, this, &SocoliteAvatarEditor::onAccept);
}

void SocoliteAvatarEditor::onColorChanged()
{
    auto *btn = qobject_cast<QPushButton *>(sender());
    QColor *target = (btn == m_bgColorBtn) ? &m_bgColor : &m_fgColor;
    QColor chosen = QColorDialog::getColor(*target, this, "Choose Color");
    if (chosen.isValid()) {
        *target = chosen;
        btn->setStyleSheet(QString("background: %1; border: 2px solid #555; border-radius: 4px;").arg(chosen.name()));
        regeneratePreview();
    }
}

void SocoliteAvatarEditor::regeneratePreview()
{
    QImage img(m_avatarSize, m_avatarSize, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);

    int s = m_avatarSize;
    int h = s / 2;

    // Background shape
    p.setBrush(m_bgColor);
    p.setPen(Qt::NoPen);
    QString bg = m_bgShape->currentText();
    if (bg == "Circle") {
        p.drawEllipse(0, 0, s, s);
    } else if (bg == "Square") {
        p.drawRoundedRect(4, 4, s - 8, s - 8, 8, 8);
    } else if (bg == "Triangle") {
        QPolygonF tri;
        tri << QPointF(h, 4) << QPointF(4, s - 4) << QPointF(s - 4, s - 4);
        p.drawPolygon(tri);
    }

    // Foreground shape
    p.setBrush(m_fgColor);
    QString fg = m_fgShape->currentText();
    int fs = s / 3;
    if (fg == "Circle") {
        p.drawEllipse(h - fs / 2, h - fs / 2, fs, fs);
    } else if (fg == "Square") {
        p.drawRoundedRect(h - fs / 2, h - fs / 2, fs, fs, 4, 4);
    } else if (fg == "Triangle") {
        QPolygonF tri;
        tri << QPointF(h, h - fs / 2) << QPointF(h - fs / 2, h + fs / 2) << QPointF(h + fs / 2, h + fs / 2);
        p.drawPolygon(tri);
    } else if (fg == "Star") {
        QPolygonF star;
        for (int i = 0; i < 5; ++i) {
            double angle = -M_PI / 2 + i * 2 * M_PI / 5;
            star << QPointF(h + (fs / 2) * cos(angle), h + (fs / 2) * sin(angle));
            angle += M_PI / 5;
            star << QPointF(h + (fs / 4) * cos(angle), h + (fs / 4) * sin(angle));
        }
        p.drawPolygon(star);
    }

    p.end();

    m_preview->setPixmap(QPixmap::fromImage(img)
        .scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

QImage SocoliteAvatarEditor::currentAvatar() const
{
    QImage img(m_avatarSize, m_avatarSize, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);

    int s = m_avatarSize;
    int h = s / 2;

    p.setBrush(m_bgColor);
    p.setPen(Qt::NoPen);
    QString bg = m_bgShape->currentText();
    if (bg == "Circle") p.drawEllipse(0, 0, s, s);
    else if (bg == "Square") p.drawRoundedRect(4, 4, s - 8, s - 8, 8, 8);
    else if (bg == "Triangle") {
        QPolygonF tri;
        tri << QPointF(h, 4) << QPointF(4, s - 4) << QPointF(s - 4, s - 4);
        p.drawPolygon(tri);
    }

    p.setBrush(m_fgColor);
    QString fg = m_fgShape->currentText();
    int fs = s / 3;
    if (fg == "Circle") p.drawEllipse(h - fs / 2, h - fs / 2, fs, fs);
    else if (fg == "Square") p.drawRoundedRect(h - fs / 2, h - fs / 2, fs, fs, 4, 4);
    else if (fg == "Triangle") {
        QPolygonF tri;
        tri << QPointF(h, h - fs / 2) << QPointF(h - fs / 2, h + fs / 2) << QPointF(h + fs / 2, h + fs / 2);
        p.drawPolygon(tri);
    } else if (fg == "Star") {
        QPolygonF star;
        for (int i = 0; i < 5; ++i) {
            double angle = -M_PI / 2 + i * 2 * M_PI / 5;
            star << QPointF(h + (fs / 2) * cos(angle), h + (fs / 2) * sin(angle));
            angle += M_PI / 5;
            star << QPointF(h + (fs / 4) * cos(angle), h + (fs / 4) * sin(angle));
        }
        p.drawPolygon(star);
    }
    p.end();
    return img;
}

void SocoliteAvatarEditor::onAccept()
{
    QImage img = currentAvatar();
    QByteArray pngData;
    QBuffer buffer(&pngData);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");

    SocoliteEngine::instance().setAvatar(pngData);
    emit avatarAccepted(img);
}
