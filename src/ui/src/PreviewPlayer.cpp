#include "PreviewPlayer.h"

#include <QVBoxLayout>
#include <QAudioOutput>

PreviewPlayer::PreviewPlayer(QWidget *parent)
    : QWidget(parent)
    , m_player(new QMediaPlayer(this))
    , m_videoWidget(new QVideoWidget(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_videoWidget);

    // Mute audio — previews are silent loops
    auto *audioOutput = new QAudioOutput(this);
    audioOutput->setVolume(0.0f);
    m_player->setAudioOutput(audioOutput);
    m_player->setVideoOutput(m_videoWidget);
    m_player->setLoops(QMediaPlayer::Infinite);

    connect(m_player, &QMediaPlayer::mediaStatusChanged,
            this, &PreviewPlayer::onPlayerStatusChanged);
}

PreviewPlayer::~PreviewPlayer() = default;

void PreviewPlayer::loadPreview(const QString &filePath)
{
    m_player->setSource(QUrl::fromLocalFile(filePath));
    m_player->play();
}

void PreviewPlayer::stopPreview()
{
    m_player->stop();
    m_player->setSource({});
}

void PreviewPlayer::onPlayerStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::InvalidMedia)
        stopPreview();
}
