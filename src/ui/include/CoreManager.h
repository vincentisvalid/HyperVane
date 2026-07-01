#pragma once

#include <QString>
#include <QVector>
#include <QHash>
#include <QPair>

struct CoreInfo {
    QString id;
    QString displayName;
    QString binaryName;
    QStringList supportedPlatforms;
    QString argsTemplate;
    int priority = 100;
};

class CoreManager
{
public:
    static CoreManager &instance();

    QString autoSelectCore(const QString &platform) const;
    QString launchCommand(const QString &romPath, const QString &platform, const QString &overrideCore = {}) const;

    bool isCoreAvailable(const QString &coreId) const;
    void setCorePath(const QString &coreId, const QString &path);
    QString corePath(const QString &coreId) const;

    void setCoreForGame(const QString &romId, const QString &coreId);
    QString coreForGame(const QString &romId) const;

    void autoDetectCores();
    QVector<QPair<QString, QString>> detectedCores() const { return m_detected; }

    QVector<CoreInfo> availableCores() const { return m_cores; }
    QVector<QPair<QString, QString>> platformSuggestions() const { return m_platformToCore; }

private:
    CoreManager();

    QVector<CoreInfo> m_cores;
    QVector<QPair<QString, QString>> m_platformToCore; // platform → best coreId
    QVector<QPair<QString, QString>> m_detected; // emulator → binary path

    QString findBinary(const QString &name) const;
};
