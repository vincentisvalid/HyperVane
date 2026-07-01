#pragma once

#include <QWidget>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QAudioOutput>
#include <QSlider>
#include <QString>
#include <memory>

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
    QAudioOutput  *m_audioOutput = nullptr;
    QSlider       *m_volumeSlider = nullptr;
};
