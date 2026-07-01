#include "IntroOverlay.h"

#include <QVBoxLayout>

IntroOverlay::IntroOverlay(const QString &videoPath, QWidget *parent)
    : QWidget(parent)
    , m_player(new QMediaPlayer(this))
    , m_video(new QVideoWidget(this))
{
    // Cover the parent completely; raise() ensures it sits above all siblings.
    setGeometry(parent ? parent->rect() : QRect(0, 0, 1600, 900));
    raise();
    setFocusPolicy(Qt::StrongFocus);
    setFocus();
    setStyleSheet("background: black;");
    show();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_video);

    auto *audio = new QAudioOutput(this);
    audio->setVolume(1.0f);
    m_player->setAudioOutput(audio);
    m_player->setVideoOutput(m_video);
    m_player->setSource(QUrl::fromLocalFile(videoPath));

    connect(m_player, &QMediaPlayer::mediaStatusChanged,
            this, &IntroOverlay::onStatusChanged);

    m_player->play();
}

void IntroOverlay::onStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::EndOfMedia || status == QMediaPlayer::InvalidMedia)
        emit finished();
}
