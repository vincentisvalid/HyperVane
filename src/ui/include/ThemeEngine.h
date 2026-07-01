#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QVector>
#include <QDir>

class ThemeEngine : public QObject
{
    Q_OBJECT

public:
    static ThemeEngine &instance();

    bool loadTheme(const QString &filePath);
    bool loadThemeByName(const QString &name);
    QStringList availableThemes() const;
    QString currentTheme() const { return m_currentTheme; }
    QString currentThemeDir() const { return m_themeDir; }

    void applyTheme(QWidget *rootWidget);

    QString toStyleSheet() const { return m_cachedQss; }

public slots:
    void switchTheme(const QString &name);

signals:
    void themeChanged(const QString &name);

private:
    ThemeEngine();
    bool parseFile(const QString &path);

    struct Property {
        QString name;
        QString value;
    };
    struct Rule {
        QString selector;
        QVector<Property> properties;
    };

    QString resolveVars(const QString &value) const;
    QString buildQss() const;

    QString m_currentTheme;
    QString m_themeDir;
    QHash<QString, QString> m_variables;
    QVector<Rule> m_rules;
    QString m_cachedQss;
};
