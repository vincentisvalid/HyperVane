#pragma once

#include <QWidget>
#include <QImage>
#include <QColor>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QComboBox>

class SocoliteAvatarEditor : public QWidget
{
    Q_OBJECT
public:
    explicit SocoliteAvatarEditor(QWidget *parent = nullptr);

    QImage currentAvatar() const;

signals:
    void avatarAccepted(const QImage &avatar);

private:
    void setupUI();
    void onColorChanged();
    void onShapeChanged();
    void regeneratePreview();
    void onAccept();

    struct Part {
        QString shape; // "circle", "square", "triangle"
        QColor color;
    };

    QLabel *m_preview = nullptr;
    QComboBox *m_bgShape = nullptr;
    QComboBox *m_fgShape = nullptr;
    QPushButton *m_bgColorBtn = nullptr;
    QPushButton *m_fgColorBtn = nullptr;
    QSlider *m_sizeSlider = nullptr;
    QColor m_bgColor = QColor("#0066cc");
    QColor m_fgColor = QColor("#ffffff");
    int m_avatarSize = 128;
};
