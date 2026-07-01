#pragma once

#include <QString>
#include <QVector>
#include <QHash>
#include <QSettings>

struct EmulatorCore {
    QString id;
    QString displayName;
    QString defaultBinary;     // e.g. "pcsx2-qt"
    QString installedPath;     // user-configured path, overrides $PATH lookup
    QStringList supportedPlatforms;
    QString launchArgs;        // e.g. "--fullscreen {rom}"
};

class EmulatorCoreConfig
{
public:
    static EmulatorCoreConfig &instance();

    QVector<EmulatorCore> availableCores() const;
    EmulatorCore coreForRom(const QString &romPath, const QString &platform) const;
    QString launchCommand(const QString &romPath, const QString &platform) const;

    void setCorePath(const QString &coreId, const QString &path);
    QString corePath(const QString &coreId) const;

    void save();
    void load();

private:
    EmulatorCoreConfig();
    QVector<EmulatorCore> m_cores;
    QSettings m_settings;
};
