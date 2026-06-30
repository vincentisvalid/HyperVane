#pragma once

#include <QWidget>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QAudioOutput>
#include <QKeyEvent>
#include <QMouseEvent>

// Fullscreen intro that plays once over the main window on startup.
// Emits finished() when the video ends or the user clicks / presses any key.
class IntroOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit IntroOverlay(const QString &videoPath, QWidget *parent = nullptr);

signals:
    void finished();

protected:
    void keyPressEvent(QKeyEvent *) override   { emit finished(); }
    void mousePressEvent(QMouseEvent *) override { emit finished(); }

private slots:
    void onStatusChanged(QMediaPlayer::MediaStatus status);

private:
    QMediaPlayer *m_player = nullptr;
    QVideoWidget *m_video  = nullptr;
};
