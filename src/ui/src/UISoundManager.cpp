#include "UISoundManager.h"
#include <QDir>

static const char *kSoundDir = HYPERVANE_ASSETS_DIR "/sounds/";

UISoundManager::UISoundManager(QObject *parent)
    : QObject(parent)
{
    load(Hover,  "hover.wav");
    load(Move,   "move.wav");
    load(Select, "select.wav");
}

void UISoundManager::load(Sound s, const QString &filename)
{
    QString full = QDir::cleanPath(QString::fromLatin1(kSoundDir) + filename);
    auto *fx = new QSoundEffect(this);
    fx->setSource(QUrl::fromLocalFile(full));
    fx->setVolume(m_volume);
    fx->setLoopCount(1);
    m_sounds.insert(s, fx);
}

void UISoundManager::play(Sound s)
{
    auto it = m_sounds.find(s);
    if (it != m_sounds.end())
        it.value()->play();
}

void UISoundManager::setVolume(qreal vol)
{
    m_volume = qBound(0.0, vol, 1.0);
    for (auto *fx : m_sounds)
        fx->setVolume(m_volume);
}

qreal UISoundManager::volume() const { return m_volume; }
