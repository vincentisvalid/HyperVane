#pragma once

#include <QObject>
#include <QHash>
#include <QSoundEffect>
#include <QString>

class UISoundManager : public QObject
{
    Q_OBJECT

public:
    explicit UISoundManager(QObject *parent = nullptr);

    enum Sound { Hover, Move, Select };

    void play(Sound s);
    void setVolume(qreal volume); // 0.0 – 1.0
    qreal volume() const;

private:
    void load(Sound s, const QString &path);

    QHash<int, QSoundEffect *> m_sounds;
    qreal m_volume = 0.5;
};
