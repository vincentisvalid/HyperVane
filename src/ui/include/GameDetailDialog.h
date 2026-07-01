#pragma once

#include <QDialog>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QVideoWidget>
#include <QMediaPlayer>
#include <QAudioOutput>
#include "RomInfo.h"

class GameDetailDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GameDetailDialog(const RomInfo &rom,
                              const QString &previewPath = {},
                              QWidget *parent = nullptr);

    const RomInfo &rom() const { return m_rom; }

signals:
    void playRequested(const QString &romId);
    void previewRequested(const QString &romId, const QString &romPath);

private slots:
    void onScreenshotClicked(int index);

private:
    void setupUI();
    void loadCoverArt();
    void populateScreenshotGallery();
    void updateThemeStyles();

    RomInfo m_rom;
    QString m_previewPath;

    QLabel     *m_coverArt      = nullptr;
    QLabel     *m_titleLabel    = nullptr;
    QLabel     *m_metaLabel     = nullptr;
    QLabel     *m_romIdLabel    = nullptr;
    QLabel     *m_fileSizeLabel = nullptr;
    QTextEdit  *m_descriptionText = nullptr;
    QListWidget *m_screenshotGallery = nullptr;
    QPushButton *m_playBtn      = nullptr;
    QPushButton *m_previewBtn   = nullptr;
    QLabel     *m_previewThumb  = nullptr;
};
