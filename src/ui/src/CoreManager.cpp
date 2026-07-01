#include "CoreManager.h"

#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

CoreManager &CoreManager::instance()
{
    static CoreManager inst;
    return inst;
}

CoreManager::CoreManager()
{
    // Priority-ordered cores (lower number = preferred)
    m_cores = {
        {"ryujinx",    "Ryujinx",    "ryujinx",           {"switch"},                      "{rom}", 10},
        {"cemu",       "Cemu",       "cemu",              {"wiiu"},                        "--fullscreen --game {rom}", 15},
        {"pcsx2",      "PCSX2",      "pcsx2-qt",          {"ps2"},                         "--fullscreen --nogui {rom}", 20},
        {"rpcs3",      "RPCS3",      "rpcs3",             {"ps3"},                         "--no-gui {rom}", 25},
        {"shadps4",    "shadPS4",    "shadps4",           {"ps4"},                         "{rom}", 30},
        {"dolphin-emu","Dolphin",    "dolphin-emu",       {"gc","wii"},                    "--batch --exec={rom}", 35},
        {"duckstation","DuckStation","duckstation-qt",    {"ps1"},                         "--fullscreen --rompath {rom}", 40},
        {"swanstation","SwanStation","swanstation",       {"ps1"},                         "{rom}", 45},
        {"melonds",    "melonDS",    "melonds",           {"nds"},                         "{rom}", 50},
        {"citra",      "Citra",      "citra-qt",          {"3ds"},                         "--fullscreen {rom}", 55},
        {"ppsspp",     "PPSSPP",     "PPSSPPSDL",         {"psp"},                         "{rom}", 60},
        {"xemu",       "Xemu",       "xemu",              {"xbox"},                        "--full-screen -dvd_path {rom}", 65},
        {"xenia",      "Xenia",      "xenia",             {"xbox360"},                     "--fullscreen {rom}", 70},
        {"flycast",    "Flycast",    "flycast",           {"dreamcast","saturn"},          "{rom}", 75},
        {"mame",       "MAME",       "mame",              {"arcade"},                      "{rom}", 80},
        {"retroarch",  "RetroArch",  "retroarch",         {"nes","snes","gba","gb","gbc",
                                                           "genesis","n64","ps1","tg16",
                                                           "ngp","ws","sms","gg"},         "--fullscreen -L {core} {rom}", 90},
    };

    // Best-emulator-per-platform map (used when multiple cores match)
    m_platformToCore = {
        {"switch",    "ryujinx"},
        {"wiiu",      "cemu"},
        {"ps2",       "pcsx2"},
        {"ps3",       "rpcs3"},
        {"ps4",       "shadps4"},
        {"gc",        "dolphin-emu"},
        {"wii",       "dolphin-emu"},
        {"ps1",       "duckstation"},
        {"nds",       "melonds"},
        {"3ds",       "citra"},
        {"psp",       "ppsspp"},
        {"xbox",      "xemu"},
        {"xbox360",   "xenia"},
        {"dreamcast", "flycast"},
        {"saturn",    "flycast"},
        {"arcade",    "mame"},
        {"nes",       "retroarch"},
        {"snes",      "retroarch"},
        {"gba",       "retroarch"},
        {"gb",        "retroarch"},
        {"gbc",       "retroarch"},
        {"genesis",   "retroarch"},
        {"n64",       "retroarch"},
        {"tg16",      "retroarch"},
        {"sms",       "retroarch"},
        {"gg",        "retroarch"},
    };
}

QString CoreManager::autoSelectCore(const QString &platform) const
{
    QString plat = platform.toLower();

    // 1. Check explicit platform→core map first
    for (const auto &pair : m_platformToCore) {
        if (pair.first == plat) {
            if (isCoreAvailable(pair.second))
                return pair.second;
        }
    }

    // 2. Fall back to priority scan
    int bestPri = 9999;
    QString best;
    for (const auto &core : m_cores) {
        if (core.supportedPlatforms.contains(plat) && core.priority < bestPri) {
            bestPri = core.priority;
            best = core.id;
        }
    }
    return best;
}

QString CoreManager::launchCommand(const QString &romPath, const QString &platform, const QString &overrideCore) const
{
    QString coreId = overrideCore.isEmpty() ? autoSelectCore(platform) : overrideCore;
    if (coreId.isEmpty()) return {};

    const CoreInfo *found = nullptr;
    for (const auto &c : m_cores) {
        if (c.id == coreId) { found = &c; break; }
    }
    if (!found) return {};

    QString binary = corePath(coreId);
    if (binary.isEmpty()) binary = found->binaryName;

    QString args = found->argsTemplate;
    args.replace("{rom}", romPath);
    return QString("\"%1\" %2 &").arg(binary, args);
}

bool CoreManager::isCoreAvailable(const QString &coreId) const
{
    QString path = corePath(coreId);
    if (!path.isEmpty() && QFileInfo::exists(path))
        return true;
    // Check default binary name in PATH
    for (const auto &c : m_cores) {
        if (c.id == coreId) {
            return !findBinary(c.binaryName).isEmpty();
        }
    }
    return false;
}

void CoreManager::setCorePath(const QString &coreId, const QString &path)
{
    QSettings s("HyperVane", "CoreManager");
    s.beginGroup("paths");
    s.setValue(coreId, path);
    s.endGroup();
    s.sync();
}

QString CoreManager::corePath(const QString &coreId) const
{
    QSettings s("HyperVane", "CoreManager");
    s.beginGroup("paths");
    QString stored = s.value(coreId).toString();
    s.endGroup();
    if (!stored.isEmpty() && QFileInfo::exists(stored))
        return stored;

    // Check default binary
    for (const auto &c : m_cores) {
        if (c.id == coreId) {
            QString found = findBinary(c.binaryName);
            if (!found.isEmpty()) return found;
        }
    }
    return {};
}

void CoreManager::setCoreForGame(const QString &romId, const QString &coreId)
{
    QSettings s("HyperVane", "CoreManager");
    s.beginGroup("game_overrides");
    s.setValue(romId, coreId);
    s.endGroup();
    s.sync();
}

QString CoreManager::coreForGame(const QString &romId) const
{
    QSettings s("HyperVane", "CoreManager");
    s.beginGroup("game_overrides");
    QString val = s.value(romId).toString();
    s.endGroup();
    return val;
}

void CoreManager::autoDetectCores()
{
    m_detected.clear();
    for (const auto &core : m_cores) {
        QString path = corePath(core.id);
        if (!path.isEmpty())
            m_detected.append({core.displayName, path});
    }
}

QString CoreManager::findBinary(const QString &name) const
{
    return QStandardPaths::findExecutable(name);
}
