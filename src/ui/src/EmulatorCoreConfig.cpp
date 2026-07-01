#include "EmulatorCoreConfig.h"

#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <QFileInfo>

EmulatorCoreConfig &EmulatorCoreConfig::instance()
{
    static EmulatorCoreConfig inst;
    return inst;
}

EmulatorCoreConfig::EmulatorCoreConfig()
    : m_settings("HyperVane", "EmulatorCoreConfig")
{
    m_cores = {
        {"pcsx2",       "PCSX2",       "pcsx2-qt",       {}, {"ps2"},                          "--fullscreen --nogui {rom}"},
        {"swanstation", "SwanStation", "swanstation",     {}, {"ps1"},                          "{rom}"},
        {"rpcs3",       "RPCS3",       "rpcs3",           {}, {"ps3"},                          "--no-gui {rom}"},
        {"shadps4",     "shadPS4",     "shadps4",         {}, {"ps4"},                          "{rom}"},
        {"retroarch",   "RetroArch",   "retroarch",       {}, {"nes","snes","gba","gb","gbc","genesis","n64","ps1"}, "--fullscreen -L {core} {rom}"},
        {"dolphin-emu", "Dolphin",     "dolphin-emu",     {}, {"gc","wii"},                     "--batch --exec={rom}"},
        {"duckstation", "DuckStation", "duckstation-qt",  {}, {"ps1"},                          "--fullscreen --rompath {rom}"},
        {"melonds",     "melonDS",     "melonds",         {}, {"nds"},                          "{rom}"},
        {"citra",       "Citra",       "citra-qt",        {}, {"3ds"},                          "--fullscreen {rom}"},
        {"ppsspp",      "PPSSPP",      "PPSSPPSDL",       {}, {"psp"},                          "{rom}"},
        {"xemu",        "Xemu",        "xemu",            {}, {"xbox"},                         "--full-screen -dvd_path {rom}"},
        {"xenia",       "Xenia",       "xenia",           {}, {"xbox360"},                      "--fullscreen {rom}"},
        {"flycast",     "Flycast",     "flycast",         {}, {"dreamcast","saturn"},           "{rom}"},
        {"mame",        "MAME",        "mame",            {}, {"arcade"},                       "{rom}"},
    };
    load();
}

QVector<EmulatorCore> EmulatorCoreConfig::availableCores() const
{
    return m_cores;
}

EmulatorCore EmulatorCoreConfig::coreForRom(const QString &romPath, const QString &platform) const
{
    QString plat = platform.toLower();
    for (const auto &core : m_cores) {
        if (core.supportedPlatforms.contains(plat))
            return core;
    }
    return {};
}

QString EmulatorCoreConfig::launchCommand(const QString &romPath, const QString &platform) const
{
    EmulatorCore core = coreForRom(romPath, platform);
    if (core.id.isEmpty())
        return {};

    QString binary = core.installedPath.isEmpty() ? core.defaultBinary : core.installedPath;
    QString args = core.launchArgs;
    args.replace("{rom}", romPath);
    return QString("\"%1\" %2 &").arg(binary, args);
}

void EmulatorCoreConfig::setCorePath(const QString &coreId, const QString &path)
{
    for (auto &core : m_cores) {
        if (core.id == coreId) {
            core.installedPath = path;
            break;
        }
    }
    save();
}

QString EmulatorCoreConfig::corePath(const QString &coreId) const
{
    for (const auto &core : m_cores) {
        if (core.id == coreId)
            return core.installedPath;
    }
    return {};
}

void EmulatorCoreConfig::save()
{
    m_settings.beginGroup("cores");
    for (const auto &core : m_cores) {
        if (!core.installedPath.isEmpty())
            m_settings.setValue(core.id, core.installedPath);
    }
    m_settings.endGroup();
    m_settings.sync();
}

void EmulatorCoreConfig::load()
{
    m_settings.beginGroup("cores");
    for (auto &core : m_cores) {
        QString path = m_settings.value(core.id).toString();
        if (!path.isEmpty())
            core.installedPath = path;
    }
    m_settings.endGroup();
}
