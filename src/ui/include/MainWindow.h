#pragma once

#include <QMainWindow>
#include <QPointer>
#include <QVector>
#include <QStackedWidget>
#include <QPushButton>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QComboBox>
#include <memory>
#include "RomInfo.h"

class LibraryView;
class PreviewPlayer;
class IPCClient;
class IntroOverlay;
class VirtualKeyboard;
class InGameMenu;
class GameDetailDialog;

class EmulatorCoreConfig;
class UISoundManager;
class ControllerInput;
class ThemeEngine;

class SocoliteFriendsList;
class SocoliteChat;
class SocoliteActivityFeed;
class SocoliteUserSearch;
class SocoliteAvatarEditor;

class QTimer;
class QListWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    void start();

    const QVector<RomInfo> &romCache() const { return m_romCache; }
    void refreshAccountInfo();

private slots:
    void onIPCConnected();
    void onConnectionLost();
    void onRomSelected(const QString &romId, const QString &romPath);
    void onPreviewReady(const QString &romId, const QString &filePath);
    void onScrapeProgress(const QString &romId, float percent, const QString &stage, const QString &message);
    void onSearchResults(const QVector<RomInfo> &roms);
    void onLaunchRequested(const QString &romId, const QString &romPath, const QString &emulatorId);
    void onRomPlayRequested(const QString &romId);
    void onRomSearchResults(const QVector<RomInfo> &results);
    void onSearchDownloadProgress(const QString &romId, qint64 received, qint64 total);
    void onSearchDownloadComplete(const QString &romId, const QString &filePath);
    void onSearchDownloadError(const QString &romId, const QString &error);
    void switchPage(int index);
    void onSearchDebounce();
    void onDetailsRequested(const QString &romId);
    void toggleTheme();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void setupNavbar();
    void setupPages();
    void setupSocialPage();
    void connectSignals();
    void showIntro();
    void showSearchSuggestions(const QString &text);
    void toggleInGameMenu();
    void resumeGame();
    void saveAndClose();
    void closeGame();
    void goHome();
    void exitHyperVane();
    void bootNextStage();
    void stageLoadBackend();
    void stagePlayIntro();
    void stageLogin();
    void stageFinalizeUI();
    RomInfo findRomInfo(const QString &romId) const;

    std::unique_ptr<UISoundManager>   m_soundManager;
    std::unique_ptr<ControllerInput>  m_controllerInput;
    QPointer<QTimer>                  m_searchDebounce;
    std::unique_ptr<LibraryView>      m_libraryView;
    std::unique_ptr<PreviewPlayer>    m_previewPlayer;
    std::unique_ptr<IPCClient>        m_ipcClient;
    std::unique_ptr<VirtualKeyboard>  m_virtualKeyboard;
    QPointer<IntroOverlay>            m_introOverlay;
    InGameMenu                       *m_ingameMenu      = nullptr;

    // Socolite social
    SocoliteFriendsList               *m_socoliteFriends     = nullptr;
    SocoliteChat                      *m_socoliteChat        = nullptr;
    SocoliteActivityFeed              *m_socoliteFeed        = nullptr;
    SocoliteUserSearch                *m_socoliteSearch      = nullptr;
    SocoliteAvatarEditor              *m_socoliteAvatarEditor = nullptr;

    // Navbar
    QFrame                           *m_navbar        = nullptr;
    QLabel                           *m_logoLabel     = nullptr;
    QPushButton                      *m_libraryBtn    = nullptr;
    QPushButton                      *m_searchBtn     = nullptr;
    QPushButton                      *m_socialBtn     = nullptr;
    QPushButton                      *m_accountBtn    = nullptr;
    QPushButton                      *m_controllerBtn = nullptr;
    QPushButton                      *m_themeToggle   = nullptr;

    // Pages
    QStackedWidget                   *m_stack         = nullptr;

    // Library page
    QFrame                           *m_heroSection   = nullptr;
    QLabel                           *m_heroTitle     = nullptr;
    QLabel                           *m_heroSubtitle  = nullptr;
    QProgressBar                     *m_downloadBar   = nullptr;

    // Search page
    QLineEdit                        *m_searchInput     = nullptr;
    QListWidget                      *m_searchSuggestions = nullptr;
    QListWidget                      *m_searchResultsList = nullptr;
    QProgressBar                     *m_searchDownloadBar = nullptr;

    // In-game state
    bool                               m_isPlaying       = false;
    QString                            m_currentRomId;
    QString                            m_currentRomPath;
    qint64                             m_currentPid      = -1;

    // Account page
    QLabel                           *m_accountName   = nullptr;
    QLabel                           *m_accountEmail  = nullptr;
    QLabel                           *m_accountAvatar = nullptr;
    QComboBox                        *m_coreCombo     = nullptr;
    QLineEdit                        *m_corePathEdit  = nullptr;
    QComboBox                        *m_themeCombo    = nullptr;

    // Detail view
    GameDetailDialog                 *m_detailDialog  = nullptr;

    // Boot state machine
    int                                m_bootStage      = -1;

    // ROM cache (kept for lookups without IPC round-trips)
    QVector<RomInfo>                   m_romCache;

    // Preview cache (romId → filePath)
    QHash<QString, QString>            m_previewPaths;
};
