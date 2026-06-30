#pragma once
#include <QString>

struct RomInfo {
    QString id;
    QString title;
    QString platform;
    QString region;
    QString filePath;
    bool    verified = false;
};
