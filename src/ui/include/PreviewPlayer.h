#pragma once

#include <QWidget>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QString>
#include <memory>

// Zero-copy video preview widget. Accepts a file path to an encoded loop
// produced by the scraper daemon and plays it in a continuous silent loop.
class PreviewPlayer : public QWidget
{
    Q_OBJECT

public:
    explicit PreviewPlayer(QWidget *parent = nullptr);
    ~PreviewPlayer() override;

public slots:
    void loadPreview(const QString &filePath);
    void stopPreview();

private slots:
    void onPlayerStatusChanged(QMediaPlayer::MediaStatus status);

private:
    QMediaPlayer  *m_player      = nullptr;
    QVideoWidget  *m_videoWidget = nullptr;
};
