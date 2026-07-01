#include "PreviewPlayer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>


PreviewPlayer::PreviewPlayer(QWidget *parent)
    : QWidget(parent)
    , m_player(new QMediaPlayer(this))
    , m_videoWidget(new QVideoWidget(this))
    , m_audioOutput(new QAudioOutput(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_videoWidget, 1);

    // Audio controls
    auto *audioBar = new QHBoxLayout;
    audioBar->setContentsMargins(8, 4, 8, 4);

    auto *muteBtn = new QPushButton("\xF0\x9F\x94\x8A");
    muteBtn->setFixedSize(28, 28);
    muteBtn->setFlat(true);
    muteBtn->setToolTip("Mute / Unmute");
    muteBtn->setStyleSheet("QPushButton { font-size: 16px; }");
    audioBar->addWidget(muteBtn);

    m_volumeSlider = new QSlider(Qt::Horizontal);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(60);
    m_volumeSlider->setFixedWidth(120);
    m_volumeSlider->setToolTip("Volume");
    audioBar->addWidget(m_volumeSlider);

    auto *volLabel = new QLabel("60%");
    volLabel->setStyleSheet("color: #888; font-size: 11px;");
    audioBar->addWidget(volLabel);

    audioBar->addStretch();
    layout->addLayout(audioBar);

    // Audio output
    m_audioOutput->setVolume(0.6f);
    m_player->setAudioOutput(m_audioOutput);
    m_player->setVideoOutput(m_videoWidget);
    m_player->setLoops(QMediaPlayer::Infinite);

    connect(m_player, &QMediaPlayer::mediaStatusChanged,
            this, &PreviewPlayer::onPlayerStatusChanged);

    connect(m_volumeSlider, &QSlider::valueChanged, this, [this, volLabel](int val) {
        float vol = val / 100.0f;
        m_audioOutput->setVolume(vol);
        volLabel->setText(QString::number(val) + "%");
    });

    connect(muteBtn, &QPushButton::clicked, this, [this, muteBtn]() {
        bool muted = m_audioOutput->isMuted();
        m_audioOutput->setMuted(!muted);
        muteBtn->setText(muted ? "\xF0\x9F\x94\x8A" : "\xF0\x9F\x94\x87");
    });
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
