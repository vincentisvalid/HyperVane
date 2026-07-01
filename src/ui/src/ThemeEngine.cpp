#include "ThemeEngine.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>
#include <QWidget>

#include <algorithm>

ThemeEngine &ThemeEngine::instance()
{
    static ThemeEngine inst;
    return inst;
}

ThemeEngine::ThemeEngine()
{
    m_themeDir = QString::fromLatin1(HYPERVANE_ASSETS_DIR "/themes");
}

// ── public API ──────────────────────────────────────────────────────────────

void ThemeEngine::switchTheme(const QString &name)
{
    if (name == m_currentTheme)
        return;
    if (loadThemeByName(name)) {
        m_currentTheme = name;
        emit themeChanged(name);
    }
}

bool ThemeEngine::loadThemeByName(const QString &name)
{
    QString path = QDir(m_themeDir).absoluteFilePath(name + ".hyprplex");
    return loadTheme(path);
}

bool ThemeEngine::loadTheme(const QString &filePath)
{
    if (!QFile::exists(filePath))
        return false;
    m_rules.clear();
    m_variables.clear();
    m_cachedQss.clear();
    if (!parseFile(filePath))
        return false;
    m_cachedQss = buildQss();
    return true;
}

QStringList ThemeEngine::availableThemes() const
{
    QDir dir(m_themeDir);
    QStringList filters = {"*.hyprplex"};
    QStringList entries = dir.entryList(filters, QDir::Files | QDir::Readable);
    for (auto &e : entries)
        e = QFileInfo(e).completeBaseName();
    return entries;
}

void ThemeEngine::applyTheme(QWidget *rootWidget)
{
    if (m_cachedQss.isEmpty())
        return;
    rootWidget->setStyleSheet(m_cachedQss);
}

// ── .hyprplex parser ───────────────────────────────────────────────────────

bool ThemeEngine::parseFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QString src;
    {
        QTextStream in(&f);
        src = in.readAll();
    }

    // Split into lines, strip comments
    QStringList lines;
    for (const auto &raw : src.split('\n')) {
        QString line = raw.trimmed();
        // Strip // comments (but not inside strings — we don't support strings)
        int ci = line.indexOf("//");
        if (ci >= 0)
            line = line.left(ci).trimmed();
        if (!line.isEmpty())
            lines << line;
    }

    // State machine
    enum State { Top, Vars, Block };
    State st = Top;
    Rule curRule;

    static const QRegularExpression varDefRe(
        R"(^\$([a-zA-Z_][a-zA-Z0-9_-]*)\s*:\s*(.*?)\s*;$)");
    static const QRegularExpression propRe(
        R"(^([a-zA-Z0-9_-]+(?:\.[a-zA-Z0-9_-]+)*)\s*:\s*(.*?)\s*;$)");
    static const QRegularExpression blockStartRe(R"(^([#.]?[a-zA-Z0-9_:-]+)\s*\{$)");
    static const QRegularExpression blockEndRe(R"(^\}$)");

    for (const auto &line : lines) {
        QRegularExpressionMatch m;

        if (st == Top || st == Vars) {
            // Variable definition
            m = varDefRe.match(line);
            if (m.hasMatch()) {
                m_variables.insert(m.captured(1), m.captured(2));
                st = Vars;
                continue;
            }
            // Block start
            m = blockStartRe.match(line);
            if (m.hasMatch()) {
                // Save previous rule if any
                if (!curRule.selector.isEmpty()) {
                    m_rules.push_back(curRule);
                    curRule = Rule();
                }
                curRule.selector = m.captured(1);
                st = Block;
                continue;
            }
            // Top-level property (comment / edge case)
            continue;
        }

        if (st == Block) {
            m = blockEndRe.match(line);
            if (m.hasMatch()) {
                m_rules.push_back(curRule);
                curRule = Rule();
                st = Top;
                continue;
            }
            m = propRe.match(line);
            if (m.hasMatch()) {
                curRule.properties.push_back(
                    {m.captured(1), m.captured(2)});
                continue;
            }
        }
    }

    // Flush trailing rule
    if (!curRule.selector.isEmpty())
        m_rules.push_back(curRule);

    return true;
}

// ── QSS generation ─────────────────────────────────────────────────────────

QString ThemeEngine::resolveVars(const QString &value) const
{
    QString result = value;
    static const QRegularExpression varRe(R"(\$([a-zA-Z_][a-zA-Z0-9_-]*))");
    QRegularExpressionMatch m;
    int pos = 0;
    while ((m = varRe.match(result, pos)).hasMatch()) {
        auto it = m_variables.constFind(m.captured(1));
        if (it != m_variables.constEnd()) {
            result.replace(m.capturedStart(), m.capturedLength(), it.value());
            pos = m.capturedStart() + it.value().length();
        } else {
            pos = m.capturedEnd();
        }
    }
    return result;
}

QString ThemeEngine::buildQss() const
{
    QString qss;
    for (const auto &rule : m_rules) {
        if (rule.properties.isEmpty())
            continue;
        qss += rule.selector + " {\n";
        for (const auto &prop : rule.properties) {
            qss += "  " + prop.name + ": " + resolveVars(prop.value) + ";\n";
        }
        qss += "}\n\n";
    }
    return qss;
}
