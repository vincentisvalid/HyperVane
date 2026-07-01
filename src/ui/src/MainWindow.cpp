#include "MainWindow.h"
#include "LibraryView.h"
#include "PreviewPlayer.h"
#include "IPCClient.h"
#include "IntroOverlay.h"
#include "AccountManager.h"
#include "ControllerWizard.h"
#include "ControllerInput.h"
#include "UISoundManager.h"
#include "ThemeEngine.h"
#include "VirtualKeyboard.h"
#include "InGameMenu.h"
#include "EmulatorCoreConfig.h"
#include "GameArtLoader.h"
#include "GameGridDelegate.h"
#include "GameDetailDialog.h"
#include "SocoliteFriendsList.h"
#include "SocoliteChat.h"
#include "SocoliteActivityFeed.h"
#include "SocoliteUserSearch.h"
#include "SocoliteAvatarEditor.h"
#include "SocoliteEngine.h"
#include "RomDownloader.h"
#include "CoreManager.h"
#include "LoginDialog.h"
#include "RegisterDialog.h"

#include <QApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QProcess>
#include <QListWidget>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

static constexpr int  kReconnectDelayMs = 3000;
static constexpr char kSocketPath[]     = "/tmp/hypervane.sock";
static constexpr char kIntroVideo[]     =
    HYPERVANE_ASSETS_DIR "/platform_intros/hypervane-intro.mp4";

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("HyperVane");
    resize(1600, 900);

    // Init theme engine — load dark by default
    auto &te = ThemeEngine::instance();
    if (!te.availableThemes().isEmpty()) {
        te.loadThemeByName("dark");
        te.applyTheme(this);
    }

    // Root structure
    auto *root = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    setupNavbar();
    m_navbar->setVisible(false);
    rootLayout->addWidget(m_navbar);

    m_stack = new QStackedWidget;
    rootLayout->addWidget(m_stack, 1);
    setCentralWidget(root);

    // Splash page shown during boot
    auto *splashPage = new QWidget;
    auto *sl = new QVBoxLayout(splashPage);
    sl->setAlignment(Qt::AlignCenter);
    sl->setSpacing(16);
    auto *splashLabel = new QLabel("HYPERVANE");
    splashLabel->setStyleSheet("font-size: 48px; font-weight: bold; color: #0066cc;");
    sl->addWidget(splashLabel);
    auto *loadingLabel = new QLabel("Loading...");
    loadingLabel->setStyleSheet("font-size: 16px; color: #666;");
    sl->addWidget(loadingLabel);
    m_stack->addWidget(splashPage);
}

void MainWindow::start()
{
    m_bootStage = 0;
    QTimer::singleShot(0, this, &MainWindow::bootNextStage);
}

void MainWindow::bootNextStage()
{
    switch (m_bootStage) {
    case 0: stageLoadBackend(); break;
    case 1: stagePlayIntro(); break;
    case 2: stageLogin(); break;
    case 3: stageFinalizeUI(); break;
    }
}

void MainWindow::stageLoadBackend()
{
    m_soundManager    = std::make_unique<UISoundManager>(this);
    m_controllerInput = std::make_unique<ControllerInput>(this);
    m_libraryView     = std::make_unique<LibraryView>(this, m_soundManager.get());
    m_previewPlayer   = std::make_unique<PreviewPlayer>(this);
    m_ipcClient       = std::make_unique<IPCClient>(this);
    m_virtualKeyboard = std::make_unique<VirtualKeyboard>(this);

    m_searchDebounce = new QTimer(this);
    m_searchDebounce->setSingleShot(true);
    m_searchDebounce->setInterval(300);

    m_ipcClient->connectToServer(kSocketPath);

    m_bootStage = 1;
    bootNextStage();
}

void MainWindow::stagePlayIntro()
{
    showIntro();
    if (m_introOverlay) {
        connect(m_introOverlay, &IntroOverlay::finished, this, [this]() {
            m_bootStage = 2;
            bootNextStage();
        });
    } else {
        m_bootStage = 2;
        bootNextStage();
    }
}

void MainWindow::stageLogin()
{
    auto &am = AccountManager::instance();
    if (am.accounts().isEmpty()) {
        RegisterDialog reg(this);
        if (reg.exec() != QDialog::Accepted) {
            QApplication::quit();
            return;
        }
    } else if (!am.isLoggedIn()) {
        LoginDialog login(this);
        if (login.exec() != QDialog::Accepted) {
            QApplication::quit();
            return;
        }
    }

    m_bootStage = 3;
    bootNextStage();
}

void MainWindow::stageFinalizeUI()
{
    // Remove splash page (always index 0)
    QWidget *splashPage = m_stack->widget(0);
    if (splashPage) {
        m_stack->removeWidget(splashPage);
        delete splashPage;
    }

    // Build real pages
    setupPages();
    refreshAccountInfo();

    // In-game menu overlay — hidden until a game is running
    m_ingameMenu = new InGameMenu("", this);
    m_ingameMenu->setVisible(false);
    m_ingameMenu->setGeometry(rect());

    connectSignals();

    // Re‑request the full ROM list — signals were not connected when the
    // initial IPC connection completed, so the server's data was dropped.
    m_ipcClient->sendSearchRequest({});

    // Sync delegate colors with the loaded theme
    auto *del = qobject_cast<GameGridDelegate *>(m_libraryView->view()->itemDelegate());
    if (del) del->setDarkMode(true);

    // Reveal navbar and switch to library
    m_navbar->setVisible(true);
    m_stack->setCurrentIndex(0);
    m_libraryBtn->setChecked(true);

    m_bootStage = -1;
}

MainWindow::~MainWindow() = default;

void MainWindow::setupNavbar()
{
    m_navbar = new QFrame(this);
    m_navbar->setObjectName("navbar");
    m_navbar->setFixedHeight(64);

    auto *navLayout = new QHBoxLayout(m_navbar);
    navLayout->setContentsMargins(24, 0, 24, 0);

    m_logoLabel = new QLabel("HYPERVANE", m_navbar);

    m_libraryBtn = new QPushButton("Library", m_navbar);
    m_libraryBtn->setCheckable(true);
    m_libraryBtn->setChecked(true);

    m_searchBtn = new QPushButton("Search", m_navbar);
    m_searchBtn->setCheckable(true);

    m_socialBtn = new QPushButton("Social", m_navbar);
    m_socialBtn->setCheckable(true);

    m_accountBtn = new QPushButton("Account", m_navbar);
    m_accountBtn->setCheckable(true);

    navLayout->addWidget(m_logoLabel);
    navLayout->addSpacing(32);
    navLayout->addWidget(m_libraryBtn);
    navLayout->addWidget(m_searchBtn);
    navLayout->addWidget(m_socialBtn);
    navLayout->addWidget(m_accountBtn);
    navLayout->addStretch();

    // Theme toggle button
    m_themeToggle = new QPushButton("\u263E", m_navbar); // ☾ / ☼
    m_themeToggle->setFixedSize(36, 36);
    m_themeToggle->setToolTip("Toggle Light / Dark theme");
    m_themeToggle->setStyleSheet(
        "QPushButton { background: transparent; color: #999; border: 1px solid #555; "
        "border-radius: 18px; font-size: 18px; }"
        "QPushButton:hover { color: #0066cc; border-color: #0066cc; }");
    navLayout->addWidget(m_themeToggle);
    connect(m_themeToggle, &QPushButton::clicked, this, &MainWindow::toggleTheme);

    // Controller button in navbar
    m_controllerBtn = new QPushButton("Controller", m_navbar);
    m_controllerBtn->setStyleSheet("background: transparent; color: #999; border: 1px solid #555; border-radius: 4px; padding: 6px 12px; font-size: 12px;");
    navLayout->addWidget(m_controllerBtn);
    connect(m_controllerBtn, &QPushButton::clicked, this, [this]() {
        ControllerWizard wiz(this);
        wiz.exec();
    });
}

void MainWindow::setupPages()
{
    // ─────────────────────────────────────────────────────────────────────
    // Page 0: Library
    // ─────────────────────────────────────────────────────────────────────
    auto *libraryPage = new QWidget;
    auto *libLayout = new QVBoxLayout(libraryPage);
    libLayout->setContentsMargins(48, 24, 48, 24);

    // Hero section
    m_heroSection = new QFrame;
    m_heroSection->setObjectName("heroSection");
    m_heroSection->setFixedHeight(400);

    auto *heroLayout = new QVBoxLayout(m_heroSection);
    heroLayout->setContentsMargins(48, 32, 48, 32);
    heroLayout->addStretch();

    m_heroTitle = new QLabel("Select a game to play");
    m_heroTitle->setObjectName("heroTitle");

    m_heroSubtitle = new QLabel("Browse your library and launch any title");
    m_heroSubtitle->setObjectName("heroSubtitle");

    heroLayout->addWidget(m_heroTitle);
    heroLayout->addWidget(m_heroSubtitle);

    libLayout->addWidget(m_heroSection);
    libLayout->addSpacing(16);

    // Download progress bar (hidden by default)
    m_downloadBar = new QProgressBar;
    m_downloadBar->setRange(0, 100);
    m_downloadBar->setValue(0);
    m_downloadBar->setVisible(false);
    m_downloadBar->setFixedHeight(6);
    m_downloadBar->setTextVisible(false);
    libLayout->addWidget(m_downloadBar);

    // Section header
    auto *sectionLabel = new QLabel("Your Games");
    sectionLabel->setObjectName("sectionHeader");
    libLayout->addWidget(sectionLabel);

    // Library content
    auto *splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(m_libraryView.get());
    splitter->addWidget(m_previewPlayer.get());
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    libLayout->addWidget(splitter, 1);

    m_stack->addWidget(libraryPage); // index 0

    // ─────────────────────────────────────────────────────────────────────
    // Page 1: Search
    // ─────────────────────────────────────────────────────────────────────
    auto *searchPage = new QWidget;
    auto *searchLayout = new QVBoxLayout(searchPage);
    searchLayout->setContentsMargins(48, 48, 48, 48);

    auto *searchHeader = new QLabel("Search & Download Games");
    searchHeader->setObjectName("title");
    searchLayout->addWidget(searchHeader);

    auto *searchDesc = new QLabel("Search your library, find new ROMs online, and download with artwork");
    searchDesc->setObjectName("subtitle");
    searchLayout->addWidget(searchDesc);
    searchLayout->addSpacing(16);

    // Search input with autocomplete popup
    auto *searchInputContainer = new QFrame;
    searchInputContainer->setObjectName("searchInputContainer");
    auto *sicLayout = new QVBoxLayout(searchInputContainer);
    sicLayout->setContentsMargins(0, 0, 0, 0);
    sicLayout->setSpacing(0);

    m_searchInput = new QLineEdit;
    m_searchInput->setPlaceholderText("Search by title, platform, or genre...");
    m_searchInput->setFixedHeight(48);
    m_searchInput->setStyleSheet(
        "QLineEdit { font-size: 18px; padding: 12px 20px; "
        "border-radius: 24px; background: #f0f0f0; border: 2px solid #ccc; }"
        "QLineEdit:focus { border-color: #0066cc; background: #fff; }");
    sicLayout->addWidget(m_searchInput);

    // Autocomplete suggestions popup
    m_searchSuggestions = new QListWidget(this);
    m_searchSuggestions->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    m_searchSuggestions->setStyleSheet(
        "QListWidget { background: #1f1f1f; border: 1px solid #444; "
        "border-radius: 8px; padding: 4px; }"
        "QListWidget::item { color: #eee; padding: 8px 12px; border-radius: 4px; }"
        "QListWidget::item:hover { background: #0066cc; color: white; }");
    m_searchSuggestions->setVisible(false);

    auto *searchRow2 = new QHBoxLayout;
    searchRow2->setContentsMargins(0, 0, 0, 0);

    // Virtual keyboard
    m_virtualKeyboard->attach(m_searchInput);
    m_virtualKeyboard->setVisible(false);
    searchRow2->addWidget(m_virtualKeyboard.get(), 0, Qt::AlignLeft);

    auto *toggleKbBtn = new QPushButton("Keyboard");
    toggleKbBtn->setStyleSheet("QPushButton { background: transparent; border: 1px solid #ccc; border-radius: 12px; padding: 6px 16px; color: #666; } QPushButton:hover { border-color: #0066cc; color: #0066cc; }");
    searchRow2->addWidget(toggleKbBtn);
    searchRow2->addStretch();

    sicLayout->addLayout(searchRow2);
    searchLayout->addWidget(searchInputContainer);

    // Download progress bar
    m_searchDownloadBar = new QProgressBar;
    m_searchDownloadBar->setRange(0, 100);
    m_searchDownloadBar->setValue(0);
    m_searchDownloadBar->setVisible(false);
    m_searchDownloadBar->setFixedHeight(6);
    m_searchDownloadBar->setTextVisible(false);
    searchLayout->addWidget(m_searchDownloadBar);

    // Search results area
    m_searchResultsList = new QListWidget;
    m_searchResultsList->setStyleSheet(
        "QListWidget { background: transparent; border: none; }"
        "QListWidget::item { padding: 4px; border: none; }");
    m_searchResultsList->setSpacing(8);
    m_searchResultsList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    searchLayout->addWidget(m_searchResultsList, 1);

    connect(toggleKbBtn, &QPushButton::clicked, this, [this]() {
        bool vis = !m_virtualKeyboard->isVisible();
        m_virtualKeyboard->setVisible(vis);
        if (vis) m_searchInput->setFocus();
    });

    connect(m_searchInput, &QLineEdit::textChanged, this, [this](const QString &text) {
        showSearchSuggestions(text);
        m_searchDebounce->start();
    });

    connect(m_searchInput, &QLineEdit::returnPressed, this, [this]() {
        m_searchSuggestions->setVisible(false);
        const QString query = m_searchInput->text().trimmed();
        if (!query.isEmpty())
            m_ipcClient->sendSearchRequest(query);
        else
            m_ipcClient->sendSearchRequest({});
        m_virtualKeyboard->setVisible(false);
    });

    // Suggestion clicked
    connect(m_searchSuggestions, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        m_searchInput->setText(item->data(Qt::UserRole).toString());
        m_searchSuggestions->setVisible(false);
        m_searchInput->setFocus();
        // Trigger search
        QString query = item->data(Qt::UserRole).toString();
        if (!query.isEmpty())
            m_ipcClient->sendSearchRequest(query);
    });

    m_stack->addWidget(searchPage); // index 1

    // ─────────────────────────────────────────────────────────────────────
    // Page 2: Account
    // ─────────────────────────────────────────────────────────────────────
    auto *accountPage = new QWidget;
    auto *accountLayout = new QVBoxLayout(accountPage);
    accountLayout->setContentsMargins(48, 48, 48, 48);

    auto *accHeader = new QLabel("Account Settings");
    accHeader->setObjectName("title");
    accountLayout->addWidget(accHeader);
    accountLayout->addSpacing(24);

    // Profile section
    auto *profileGroup = new QGroupBox("Profile");
    profileGroup->setObjectName("profileCard");
    auto *profileForm = new QFormLayout(profileGroup);
    profileForm->setSpacing(12);

    m_accountAvatar = new QLabel;
    m_accountAvatar->setFixedSize(80, 80);

    m_accountName = new QLabel;
    m_accountName->setObjectName("title");

    m_accountEmail = new QLabel;
    m_accountEmail->setObjectName("subtitle");

    profileForm->addRow("Avatar:", m_accountAvatar);
    profileForm->addRow("Name:", m_accountName);
    profileForm->addRow("Email:", m_accountEmail);

    accountLayout->addWidget(profileGroup);
    accountLayout->addSpacing(16);

    // Preferences section
    auto *prefGroup = new QGroupBox("Preferences");
    auto *prefForm = new QFormLayout(prefGroup);
    prefForm->setSpacing(12);

    auto *emulatorPath = new QLineEdit;
    emulatorPath->setPlaceholderText("/path/to/emulators");
    prefForm->addRow("Emulator Path:", emulatorPath);

    auto *romPath = new QLineEdit;
    romPath->setPlaceholderText("/path/to/roms");
    prefForm->addRow("ROM Directory:", romPath);

    accountLayout->addWidget(prefGroup);

    // Emulator cores section
    accountLayout->addSpacing(16);
    auto *coresGroup = new QGroupBox("Emulator Cores");
    auto *coresLayout = new QVBoxLayout(coresGroup);

    m_coreCombo = new QComboBox;
    auto &coreConfig = EmulatorCoreConfig::instance();
    for (const auto &core : coreConfig.availableCores()) {
        m_coreCombo->addItem(core.displayName, core.id);
    }
    coresLayout->addWidget(new QLabel("Select core to configure:"));
    coresLayout->addWidget(m_coreCombo);

    auto *corePathRow = new QHBoxLayout;
    m_corePathEdit = new QLineEdit;
    m_corePathEdit->setPlaceholderText("Path to emulator binary (leave empty for $PATH)");
    corePathRow->addWidget(m_corePathEdit);
    auto *saveCoreBtn = new QPushButton("Save");
    saveCoreBtn->setStyleSheet("QPushButton { background: #0066cc; color: white; border: none; border-radius: 4px; padding: 6px 16px; }");
    corePathRow->addWidget(saveCoreBtn);
    coresLayout->addLayout(corePathRow);

    connect(m_coreCombo, &QComboBox::currentIndexChanged, this, [&]() {
        QString id = m_coreCombo->currentData().toString();
        m_corePathEdit->setText(coreConfig.corePath(id));
    });
    connect(saveCoreBtn, &QPushButton::clicked, this, [&]() {
        QString id = m_coreCombo->currentData().toString();
        coreConfig.setCorePath(id, m_corePathEdit->text());
    });

    // Auto-detect cores button
    auto *autoDetectBtn = new QPushButton("Auto-Detect Cores");
    autoDetectBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #00cc66; border: 1px solid #00cc66; "
        "border-radius: 6px; padding: 8px 20px; font-size: 13px; }"
        "QPushButton:hover { background: rgba(0,204,102,0.1); }");
    coresLayout->addWidget(autoDetectBtn);

    connect(autoDetectBtn, &QPushButton::clicked, this, [this]() {
        auto &cm = CoreManager::instance();
        cm.autoDetectCores();

        // Update the combo with detected cores
        m_coreCombo->clear();
        auto &coreCfg = EmulatorCoreConfig::instance();
        for (const auto &c : coreCfg.availableCores()) {
            m_coreCombo->addItem(c.displayName, c.id);
        }

        // Show a brief status
        auto found = cm.detectedCores();
        QStringList names;
        for (const auto &pair : found) {
            names << pair.first;
            coreCfg.setCorePath(pair.first, pair.second);
        }
        if (names.isEmpty()) {
            m_heroSubtitle->setText("No emulator cores found on this system");
        } else {
            m_heroSubtitle->setText("Detected: " + names.join(", "));
        }
        m_heroTitle->setText("Core Detection");
        QTimer::singleShot(3000, this, [this]() {
            m_heroTitle->setText("Now Playing");
            m_heroSubtitle->setText("Select a game to preview");
        });
    });

    accountLayout->addWidget(coresGroup);

    // Theme selection
    accountLayout->addSpacing(16);
    auto *themeGroup = new QGroupBox("Theme");
    auto *themeLayout = new QHBoxLayout(themeGroup);
    auto *themeLabel = new QLabel("Appearance:");
    m_themeCombo = new QComboBox;
    auto &te = ThemeEngine::instance();
    for (const auto &t : te.availableThemes())
        m_themeCombo->addItem(t);
    m_themeCombo->setCurrentText(te.currentTheme());
    themeLayout->addWidget(themeLabel);
    themeLayout->addWidget(m_themeCombo, 1);
    accountLayout->addWidget(themeGroup);

    // Controller setup button
    accountLayout->addSpacing(16);
    auto *controllerBtn = new QPushButton("Setup Controller");
    controllerBtn->setStyleSheet("QPushButton { background: #0066cc; color: white; border: none; border-radius: 8px; padding: 12px 24px; font-size: 14px; } QPushButton:hover { background: #0052a3; }");
    accountLayout->addWidget(controllerBtn);
    connect(controllerBtn, &QPushButton::clicked, this, [this]() {
        ControllerWizard wiz(this);
        wiz.exec();
    });

    // Sign out button
    accountLayout->addSpacing(24);
    auto *signOutBtn = new QPushButton("Sign Out");
    signOutBtn->setObjectName("danger");
    accountLayout->addWidget(signOutBtn);
    accountLayout->addStretch();

    m_stack->addWidget(accountPage); // index 2

    // ─────────────────────────────────────────────────────────────────────
    // Page 3: Social (Socolite)
    // ─────────────────────────────────────────────────────────────────────
    auto *socialPage = new QWidget;
    auto *socialLayout = new QVBoxLayout(socialPage);
    socialLayout->setContentsMargins(0, 0, 0, 0);
    socialLayout->setSpacing(0);

    auto *socialHeader = new QWidget;
    auto *shLayout = new QHBoxLayout(socialHeader);
    shLayout->setContentsMargins(48, 16, 48, 8);
    auto *socialTitle = new QLabel("Social · Socolite");
    socialTitle->setObjectName("title");
    shLayout->addWidget(socialTitle);
    shLayout->addStretch();

    auto *onlineStatus = new QLabel("● Online");
    onlineStatus->setStyleSheet("color: #00cc66; font-size: 13px;");
    shLayout->addWidget(onlineStatus);

    socialLayout->addWidget(socialHeader);

    // Splitter: friends list on left, content tabs on right
    auto *socialSplitter = new QSplitter(Qt::Horizontal);

    // Left: Friends list
    auto *friendsWidget = new QWidget;
    auto *friendsLayout = new QVBoxLayout(friendsWidget);
    friendsLayout->setContentsMargins(16, 0, 8, 16);
    m_socoliteFriends = new SocoliteFriendsList;
    friendsLayout->addWidget(m_socoliteFriends);
    socialSplitter->addWidget(friendsWidget);

    // Right: Tabbed content
    auto *tabWidget = new QTabWidget;
    tabWidget->setStyleSheet(
        "QTabWidget::pane { background: transparent; border: none; }"
        "QTabBar::tab { background: rgba(255,255,255,0.05); color: #aaa; "
        "  border: none; padding: 10px 20px; font-size: 12px; font-weight: 500; }"
        "QTabBar::tab:selected { color: #0066cc; border-bottom: 2px solid #0066cc; }"
        "QTabBar::tab:hover { color: #eee; }");

    m_socoliteFeed = new SocoliteActivityFeed;
    tabWidget->addTab(m_socoliteFeed, "Activity");

    auto *chatTab = new QWidget;
    auto *chatTabLayout = new QVBoxLayout(chatTab);
    chatTabLayout->setContentsMargins(8, 8, 8, 8);
    m_socoliteChat = new SocoliteChat;
    chatTabLayout->addWidget(m_socoliteChat);
    tabWidget->addTab(chatTab, "Chat");

    m_socoliteSearch = new SocoliteUserSearch;
    tabWidget->addTab(m_socoliteSearch, "Search Users");

    m_socoliteAvatarEditor = new SocoliteAvatarEditor;
    tabWidget->addTab(m_socoliteAvatarEditor, "Avatar Editor");

    socialSplitter->addWidget(tabWidget);
    socialSplitter->setStretchFactor(0, 1);
    socialSplitter->setStretchFactor(1, 3);
    socialSplitter->setSizes({250, 700});

    socialLayout->addWidget(socialSplitter, 1);
    m_stack->addWidget(socialPage); // index 3

}

void MainWindow::switchPage(int index)
{
    m_stack->setCurrentIndex(index);
    m_libraryBtn->setChecked(index == 0);
    m_searchBtn->setChecked(index == 1);
    m_socialBtn->setChecked(index == 3);
    m_accountBtn->setChecked(index == 2);
    if (index == 2)
        refreshAccountInfo();
    m_soundManager->play(UISoundManager::Select);
}

void MainWindow::refreshAccountInfo()
{
    auto &am = AccountManager::instance();
    if (am.isLoggedIn()) {
        auto acc = am.currentAccount();
        m_accountName->setText(acc.username);
        m_accountEmail->setText(acc.email.isEmpty() ? "Not set" : acc.email);
        QPixmap avatar = am.avatarPixmap(acc.id);
        m_accountAvatar->setPixmap(
            avatar.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void MainWindow::connectSignals()
{
    // ── Navbar page switching ──────────────────────────────────────────────
    connect(m_libraryBtn, &QPushButton::clicked, this, [this] { switchPage(0); });
    connect(m_searchBtn,  &QPushButton::clicked, this, [this] { switchPage(1); });
    connect(m_socialBtn,  &QPushButton::clicked, this, [this] { switchPage(3); });
    connect(m_accountBtn, &QPushButton::clicked, this, [this] { switchPage(2); });

    connect(m_ipcClient.get(), &IPCClient::connected,
            this, &MainWindow::onIPCConnected);
    connect(m_ipcClient.get(), &IPCClient::connectionLost,
            this, &MainWindow::onConnectionLost);
    connect(m_ipcClient.get(), &IPCClient::searchResultsReceived,
            this, &MainWindow::onSearchResults);
    connect(m_ipcClient.get(), &IPCClient::previewReady,
            this, &MainWindow::onPreviewReady);
    connect(m_libraryView.get(), &LibraryView::romSelected,
            this, &MainWindow::onRomSelected);
    connect(m_libraryView.get(), &LibraryView::launchRequested,
            this, &MainWindow::onLaunchRequested);

    // ── Detail view request from grid ───────────────────────────────────
    auto *delegate = qobject_cast<GameGridDelegate *>(m_libraryView->view()->itemDelegate());
    if (delegate) {
        connect(delegate, &GameGridDelegate::detailsRequested,
                this, &MainWindow::onDetailsRequested);
    }

    // ── Theme switching ──────────────────────────────────────────────
    connect(m_themeCombo, &QComboBox::currentTextChanged, this, [this](const QString &name) {
        auto &te = ThemeEngine::instance();
        if (te.currentTheme() != name && te.loadThemeByName(name)) {
            te.applyTheme(this);
            // Update delegate colors
            bool dark = name.toLower().contains("dark");
            auto *del = qobject_cast<GameGridDelegate *>(m_libraryView->view()->itemDelegate());
            if (del) del->setDarkMode(dark);
            m_soundManager->play(UISoundManager::Select);
        }
    });

    // ── Search-as-you-type (debounce also set in setupPages)
    connect(m_searchDebounce, &QTimer::timeout, this, &MainWindow::onSearchDebounce);

    // ── Search download progress (from RomDownloader) ────────────────
    connect(&RomDownloader::instance(), &RomDownloader::searchResults,
            this, &MainWindow::onRomSearchResults);
    connect(&RomDownloader::instance(), &RomDownloader::downloadProgress,
            this, &MainWindow::onSearchDownloadProgress);
    connect(&RomDownloader::instance(), &RomDownloader::downloadComplete,
            this, &MainWindow::onSearchDownloadComplete);
    connect(&RomDownloader::instance(), &RomDownloader::downloadError,
            this, &MainWindow::onSearchDownloadError);

    // ── Scrape progress ──────────────────────────────────────────────
    connect(m_ipcClient.get(), &IPCClient::scrapeProgress,
            this, &MainWindow::onScrapeProgress);

    // ── Play button from library grid ────────────────────────────────
    connect(m_libraryView.get(), &LibraryView::playRequested,
            this, &MainWindow::onRomPlayRequested);

    // When real box art finishes downloading, repaint the library grid
    connect(&GameArtLoader::instance(), &GameArtLoader::artReady,
            this, [this](const QString &) {
        m_libraryView->update();
    });

    // ── Social (Socolite) ─────────────────────────────────────────────
    // Connect to social server once IPC is connected
    connect(m_ipcClient.get(), &IPCClient::connected, this, [this]() {
        SocoliteEngine::instance().setLocalUser("local", "You");
        SocoliteEngine::instance().connectToServer("/tmp/hypervane-social.sock");
    });

    // Friends list → chat
    connect(m_socoliteFriends, &SocoliteFriendsList::friendSelected,
            this, [this](const QString &friendId) {
        m_soundManager->play(UISoundManager::Select);
        auto friends = SocoliteEngine::instance().friends();
        for (const auto &f : friends) {
            if (f.id == friendId) {
                m_socoliteChat->setFriend(friendId, f.displayName.isEmpty() ? f.username : f.displayName);
                break;
            }
        }
    });

    // Chat messages
    connect(&SocoliteEngine::instance(), &SocoliteEngine::messageReceived,
            this, [this](const SocoliteMessage &msg) {
        if (msg.senderId != SocoliteEngine::instance().localUserId())
            m_socoliteChat->addMessage(msg);
    });

    // Activity feed
    connect(&SocoliteEngine::instance(), &SocoliteEngine::activityAdded,
            m_socoliteFeed, &SocoliteActivityFeed::addActivity);

    // Avatar changes
    connect(m_socoliteAvatarEditor, &SocoliteAvatarEditor::avatarAccepted,
            this, [this](const QImage &) {
        m_soundManager->play(UISoundManager::Select);
    });

    // ── In-Game Menu ─────────────────────────────────────────────────
    connect(m_ingameMenu, &InGameMenu::resumeGame, this, &MainWindow::resumeGame);
    connect(m_ingameMenu, &InGameMenu::saveAndClose, this, &MainWindow::saveAndClose);
    connect(m_ingameMenu, &InGameMenu::closeGame, this, &MainWindow::closeGame);
    connect(m_ingameMenu, &InGameMenu::goHome, this, &MainWindow::goHome);
    connect(m_ingameMenu, &InGameMenu::exitHyperVane, this, &MainWindow::exitHyperVane);

    // ── Controller input ─────────────────────────────────────────────
    if (m_controllerInput->isAvailable()) {
        connect(m_controllerInput.get(), &ControllerInput::actionTriggered,
                this, [this](ControllerInput::Action a) {
            m_soundManager->play(UISoundManager::Move);
            switch (a) {
            case ControllerInput::Select:
                m_soundManager->play(UISoundManager::Select);
                // Simulate a click on the current item
                if (auto *view = m_libraryView->view()) {
                    auto idx = view->currentIndex();
                    if (idx.isValid())
                        emit m_libraryView->romSelected(
                            idx.data(Qt::UserRole).toString(),
                            idx.data(Qt::UserRole + 1).toString());
                }
                break;
            case ControllerInput::Back:
                // Navigate to previous page (Library as default)
                switchPage(0);
                break;
            case ControllerInput::Menu:
                toggleInGameMenu();
                break;
            default:
                break;
            }
        });
    }
}

void MainWindow::showIntro()
{
    const QString path = QString::fromLatin1(kIntroVideo);
    if (!QFile::exists(path))
        return;

    m_introOverlay = new IntroOverlay(path, this);
    connect(m_introOverlay, &IntroOverlay::finished,
            m_introOverlay, &QWidget::deleteLater);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_introOverlay)
        m_introOverlay->setGeometry(rect());
    if (m_ingameMenu)
        m_ingameMenu->setGeometry(rect());
}

void MainWindow::onIPCConnected()
{
    m_ipcClient->sendSearchRequest({});
}

void MainWindow::onConnectionLost()
{
    m_libraryView->setRoms({});
    m_previewPlayer->stopPreview();
    setWindowTitle("HyperVane — reconnecting...");
    QTimer::singleShot(kReconnectDelayMs, this, [this] {
        setWindowTitle("HyperVane");
        m_ipcClient->connectToServer(kSocketPath);
    });
}

void MainWindow::onSearchResults(const QVector<RomInfo> &roms)
{
    m_romCache = roms;
    m_previewPaths.clear();
    m_libraryView->setRoms(roms);

    // Pre-populate art cache so GameGridDelegate::paint doesn't need to
    // create QPixmap objects inside a paint event (which can crash on Wayland).
    for (const auto &rom : roms)
        GameArtLoader::instance().artFor(rom.id, rom.title, rom.platform);
}

void MainWindow::onRomSelected(const QString &romId, const QString &romPath)
{
    m_isPlaying = true;
    m_currentRomId = romId;
    m_currentRomPath = romPath;
    m_ingameMenu->setGameTitle(romId);

    m_heroTitle->setText("Now Playing");
    m_heroSubtitle->setText("Loading preview...");
    m_downloadBar->setVisible(true);
    m_downloadBar->setValue(0);
    m_ipcClient->sendLaunchRequest(romId, romPath, "retroarch");
}

void MainWindow::onPreviewReady(const QString &romId, const QString &filePath)
{
    m_previewPaths[romId] = filePath;
    m_downloadBar->setVisible(false);
    m_previewPlayer->loadPreview(filePath);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // Escape toggles the in-game menu overlay (only when a game is running)
    if (event->key() == Qt::Key_Escape) {
        toggleInGameMenu();
        event->accept();
        return;
    }

    // Ctrl+Tab cycles forward through pages, Ctrl+Shift+Tab cycles backward
    if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_Tab) {
        int next = (m_stack->currentIndex() + 1) % m_stack->count();
        switchPage(next);
        event->accept();
        return;
    }
    if (event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) && event->key() == Qt::Key_Tab) {
        int prev = (m_stack->currentIndex() - 1 + m_stack->count()) % m_stack->count();
        switchPage(prev);
        event->accept();
        return;
    }
    // Let LibraryView / QListView handle arrow keys for grid navigation
    QMainWindow::keyPressEvent(event);
}

void MainWindow::onSearchDebounce()
{
    const QString query = m_searchInput->text().trimmed();
    if (!query.isEmpty())
        m_ipcClient->sendSearchRequest(query);
    else
        m_ipcClient->sendSearchRequest({}); // empty = show all
}

void MainWindow::onDetailsRequested(const QString &romId)
{
    RomInfo info = findRomInfo(romId);
    if (info.id.isEmpty())
        return;

    QString previewPath = m_previewPaths.value(romId);

    if (m_detailDialog) {
        m_detailDialog->close();
        m_detailDialog->deleteLater();
    }

    m_detailDialog = new GameDetailDialog(info, previewPath, this);
    m_detailDialog->setAttribute(Qt::WA_DeleteOnClose, true);

    connect(m_detailDialog, &GameDetailDialog::playRequested,
            this, &MainWindow::onRomPlayRequested);

    connect(m_detailDialog, &GameDetailDialog::previewRequested,
            this, [this](const QString &romId, const QString &romPath) {
        // If we already have a preview, play it; otherwise request one
        if (m_previewPaths.contains(romId))
            m_previewPlayer->loadPreview(m_previewPaths[romId]);
        else
            onRomSelected(romId, romPath);
        m_detailDialog->close();
    });

    m_detailDialog->show();
}

RomInfo MainWindow::findRomInfo(const QString &romId) const
{
    for (const auto &info : m_romCache) {
        if (info.id == romId)
            return info;
    }
    // Fall back to the library view model
    auto *view = m_libraryView->view();
    if (view && view->model()) {
        for (int i = 0; i < view->model()->rowCount(); ++i) {
            QModelIndex idx = view->model()->index(i, 0);
            if (idx.data(Qt::UserRole).toString() == romId) {
                RomInfo info;
                info.id = romId;
                info.title = idx.data(Qt::DisplayRole).toString();
                info.filePath = idx.data(Qt::UserRole + 1).toString();
                info.platform = idx.data(Qt::UserRole + 2).toString();
                return info;
            }
        }
    }
    return {};
}

void MainWindow::toggleTheme()
{
    auto &te = ThemeEngine::instance();
    QStringList themes = te.availableThemes();
    if (themes.size() < 2)
        return;

    QString current = te.currentTheme();
    int idx = themes.indexOf(current);
    QString next = themes[(idx + 1) % themes.size()];

    if (te.loadThemeByName(next)) {
        te.switchTheme(next);
        te.applyTheme(this);

        if (m_themeCombo)
            m_themeCombo->setCurrentText(next);

        // Update delegate colors
        bool dark = next.toLower().contains("dark");
        auto *del = qobject_cast<GameGridDelegate *>(m_libraryView->view()->itemDelegate());
        if (del) del->setDarkMode(dark);

        // Update theme toggle icon
        m_themeToggle->setText(dark ? QStringLiteral("\u263E") : QStringLiteral("\u2600"));

        m_soundManager->play(UISoundManager::Select);
    }
}

void MainWindow::onRomPlayRequested(const QString &romId)
{
    m_soundManager->play(UISoundManager::Select);
    // Find the rom info and launch directly
    auto *view = m_libraryView->view();
    for (int i = 0; i < view->model()->rowCount(); ++i) {
        QModelIndex idx = view->model()->index(i, 0);
        if (idx.data(Qt::UserRole).toString() == romId) {
            QString romPath = idx.data(Qt::UserRole + 1).toString();
            onLaunchRequested(romId, romPath, "retroarch");
            break;
        }
    }
}

void MainWindow::onScrapeProgress(const QString &romId,
                                   float percent,
                                   const QString &stage,
                                   const QString &message)
{
    Q_UNUSED(romId)
    Q_UNUSED(message)
    m_downloadBar->setVisible(true);
    m_downloadBar->setValue(static_cast<int>(percent));
    if (stage == "done" || stage == "error") {
        QTimer::singleShot(1000, this, [this]() {
            m_downloadBar->setVisible(false);
        });
    }
}

void MainWindow::onLaunchRequested(const QString &romId,
                                   const QString &romPath,
                                   const QString &emulatorId)
{
    // Track game state for the in-game menu
    m_isPlaying = true;
    m_currentRomId = romId;
    m_currentRomPath = romPath;
    m_currentPid = -1;

    // Extract a friendly title from the ROM path for the menu
    QString gameTitle = romId;
    int lastSlash = romPath.lastIndexOf('/');
    int dot = romPath.lastIndexOf('.');
    if (lastSlash >= 0 && dot > lastSlash)
        gameTitle = romPath.mid(lastSlash + 1, dot - lastSlash - 1);
    else if (lastSlash >= 0)
        gameTitle = romPath.mid(lastSlash + 1);
    gameTitle.replace('_', ' ').replace('-', ' ');
    m_ingameMenu->setGameTitle(gameTitle);

    // Launch the emulator using EmulatorCoreConfig
    auto &coreConfig = EmulatorCoreConfig::instance();
    QString platform = "ps2";
    // Try to extract platform from the rom path
    if (romPath.contains("/snes/")) platform = "snes";
    else if (romPath.contains("/genesis/")) platform = "genesis";
    else if (romPath.contains("/ps1/")) platform = "ps1";
    else if (romPath.contains("/ps2/")) platform = "ps2";
    else if (romPath.contains("/ps3/")) platform = "ps3";
    else if (romPath.contains("/ps4/")) platform = "ps4";
    else if (romPath.contains("/n64/")) platform = "n64";
    else if (romPath.contains("/gba/")) platform = "gba";
    else if (romPath.contains("/dreamcast/")) platform = "dreamcast";
    else if (romPath.contains("/gc/") || romPath.contains("/gamecube/")) platform = "gc";
    else if (romPath.contains("/wii/")) platform = "wii";
    else if (romPath.contains("/nds/")) platform = "nds";
    else if (romPath.contains("/3ds/")) platform = "3ds";
    else if (romPath.contains("/psp/")) platform = "psp";
    else if (romPath.contains("/xbox/")) platform = "xbox";
    else if (romPath.contains("/xbox360/")) platform = "xbox360";
    else if (romPath.contains("/saturn/")) platform = "saturn";
    else if (romPath.contains("/arcade/")) platform = "arcade";

    QString cmd = coreConfig.launchCommand(romPath, platform);
    if (!cmd.isEmpty()) {
        qint64 pid = 0;
        QProcess::startDetached("/bin/sh", {"-c", cmd}, {}, &pid);
        m_currentPid = static_cast<qint64>(pid);
    } else {
        // Fall back to IPC launch request for the mock server
        m_ipcClient->sendLaunchRequest(romId, romPath, emulatorId);
    }
}

void MainWindow::showSearchSuggestions(const QString &text)
{
    if (text.length() < 2) {
        m_searchSuggestions->setVisible(false);
        return;
    }

    m_searchSuggestions->clear();
    const QString lower = text.toLower();

    // Use current library ROM titles as suggestions
    auto *view = m_libraryView->view();
    if (!view || !view->model()) return;

    int count = 0;
    for (int i = 0; i < view->model()->rowCount(); ++i) {
        QString title = view->model()->index(i, 0).data(Qt::DisplayRole).toString();
        if (title.toLower().contains(lower)) {
            auto *item = new QListWidgetItem(title);
            item->setData(Qt::UserRole, title);
            m_searchSuggestions->addItem(item);
            if (++count >= 6) break;
        }
    }

    if (count == 0) {
        m_searchSuggestions->setVisible(false);
        return;
    }

    // Position the popup below the search input
    QPoint pos = m_searchInput->mapToGlobal(QPoint(0, m_searchInput->height() + 4));
    int popupWidth = qMax(m_searchInput->width(), 300);
    int popupHeight = qMin(count * 36 + 8, 240);
    m_searchSuggestions->setGeometry(pos.x(), pos.y(), popupWidth, popupHeight);
    m_searchSuggestions->setVisible(true);
}

void MainWindow::onRomSearchResults(const QVector<RomInfo> &results)
{
    m_searchResultsList->clear();

    if (results.isEmpty()) {
        auto *emptyItem = new QListWidgetItem("  No results found");
        emptyItem->setFlags(Qt::NoItemFlags);
        emptyItem->setForeground(QColor("#888"));
        m_searchResultsList->addItem(emptyItem);
        return;
    }

    for (const auto &rom : results) {
        auto *widget = new QFrame;
        widget->setStyleSheet(
            "QFrame { background: rgba(255,255,255,0.04); border-radius: 8px; "
            "border: 1px solid rgba(255,255,255,0.06); }"
            "QFrame:hover { background: rgba(255,255,255,0.08); "
            "border-color: #0066cc; }");
        auto *hLayout = new QHBoxLayout(widget);
        hLayout->setContentsMargins(12, 8, 12, 8);
        hLayout->setSpacing(12);

        // Thumbnail / box art placeholder
        auto *artLabel = new QLabel;
        artLabel->setFixedSize(72, 96);
        artLabel->setStyleSheet(
            "background: #2a2a2a; border-radius: 6px; "
            "border: 1px solid #444; font-size: 28px;");
        artLabel->setAlignment(Qt::AlignCenter);
        artLabel->setText(QStringLiteral("game"));
        hLayout->addWidget(artLabel);

        // Load box art from GameArtLoader if available
        GameArtLoader::instance().requestArt(rom.platform, rom.title,
            [artLabel](const QPixmap &pix) {
                if (!pix.isNull())
                    artLabel->setPixmap(pix.scaled(72, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            });

        // Info section
        auto *infoLayout = new QVBoxLayout;
        infoLayout->setSpacing(4);

        auto *titleLabel = new QLabel(rom.title);
        titleLabel->setStyleSheet("font-size: 16px; font-weight: 600; color: #eee;");
        infoLayout->addWidget(titleLabel);

        auto *metaLabel = new QLabel(
            QString("%1  |  %2").arg(rom.platform, rom.region.isEmpty() ? "Unknown" : rom.region));
        metaLabel->setStyleSheet("font-size: 12px; color: #888;");
        infoLayout->addWidget(metaLabel);

        hLayout->addLayout(infoLayout, 1);

        // Download / Play button
        auto *actionBtn = new QPushButton;
        actionBtn->setFixedSize(100, 36);
        actionBtn->setCursor(Qt::PointingHandCursor);

        bool inLibrary = false;
        auto *lv = m_libraryView->view();
        if (lv && lv->model()) {
            for (int i = 0; i < lv->model()->rowCount(); ++i) {
                if (lv->model()->index(i, 0).data(Qt::UserRole).toString() == rom.id) {
                    inLibrary = true;
                    break;
                }
            }
        }

        if (inLibrary) {
            actionBtn->setText("\u25B6 Play");
            actionBtn->setStyleSheet(
                "QPushButton { background: #00cc66; color: white; border: none; "
                "border-radius: 18px; font-size: 13px; font-weight: 600; }"
                "QPushButton:hover { background: #00e673; }");

            QString rid = rom.id;
            connect(actionBtn, &QPushButton::clicked, this, [this, rid]() {
                onRomPlayRequested(rid);
            });
        } else {
            actionBtn->setText("Get ROM");
            actionBtn->setStyleSheet(
                "QPushButton { background: #0066cc; color: white; border: none; "
                "border-radius: 18px; font-size: 13px; font-weight: 600; }"
                "QPushButton:hover { background: #0077ee; }");

            QString rid = rom.id;
            QString platform = rom.platform;
            connect(actionBtn, &QPushButton::clicked, this, [this, rid, platform]() {
                // Build a download URL based on platform
                QString safePlatform = platform.toLower().trimmed();
                QString url = QString("http://localhost:8080/api/roms/download/%1/%2")
                    .arg(safePlatform, rid);
                QString destPath = QString("%1/%2/%3.rom")
                    .arg(QDir::homePath(), "HyperVane/roms", safePlatform)
                    .arg(rid);
                RomDownloader::instance().downloadRom(url, destPath, rid);
            });
        }

        hLayout->addWidget(actionBtn);

        auto *item = new QListWidgetItem;
        item->setSizeHint(widget->sizeHint());
        m_searchResultsList->addItem(item);
        m_searchResultsList->setItemWidget(item, widget);
    }
}

void MainWindow::onSearchDownloadProgress(const QString &romId, qint64 received, qint64 total)
{
    Q_UNUSED(romId)
    m_searchDownloadBar->setVisible(true);
    int pct = (total > 0) ? static_cast<int>((received * 100) / total) : 0;
    m_searchDownloadBar->setValue(pct);
}

void MainWindow::onSearchDownloadComplete(const QString &romId, const QString &filePath)
{
    Q_UNUSED(romId)
    m_searchDownloadBar->setValue(100);
    QTimer::singleShot(1500, this, [this]() {
        m_searchDownloadBar->setVisible(false);
    });
    // Refresh library
    m_ipcClient->sendSearchRequest({});
    // Refresh the search results UI to show Play instead of Get ROM
    onRomSearchResults({});
}

void MainWindow::onSearchDownloadError(const QString &romId, const QString &error)
{
    Q_UNUSED(romId)
    m_searchDownloadBar->setVisible(false);
    // Show a toast-like message using a temp label
    auto *errLabel = new QLabel("Download failed: " + error);
    errLabel->setStyleSheet("color: #ff4444; font-size: 12px; padding: 4px;");
    errLabel->setAlignment(Qt::AlignCenter);
    m_searchResultsList->addItem(new QListWidgetItem);
    int lastRow = m_searchResultsList->count() - 1;
    if (lastRow >= 0) {
        auto *item = m_searchResultsList->item(lastRow);
        item->setSizeHint(QSize(0, 30));
        m_searchResultsList->setItemWidget(item, errLabel);
    }
    QTimer::singleShot(3000, this, [this, errLabel]() {
        errLabel->deleteLater();
    });
}

// ── In-Game Menu Actions ───────────────────────────────────────────────

void MainWindow::toggleInGameMenu()
{
    if (!m_isPlaying)
        return;

    bool visible = !m_ingameMenu->isVisible();
    m_ingameMenu->setVisible(visible);
    m_soundManager->play(UISoundManager::Select);

    if (visible)
        m_ingameMenu->raise();
}

void MainWindow::resumeGame()
{
    m_ingameMenu->setVisible(false);
    m_soundManager->play(UISoundManager::Select);
}

void MainWindow::saveAndClose()
{
    closeGame();
}

void MainWindow::closeGame()
{
    m_ingameMenu->setVisible(false);

    // Kill the emulator process if we know its PID
    if (m_currentPid > 0) {
        QProcess::startDetached("/bin/kill", {"-s", "TERM", QString::number(m_currentPid)});
        // Give it a moment, then force-kill
        QTimer::singleShot(2000, this, [this]() {
            if (m_currentPid > 0)
                QProcess::startDetached("/bin/kill", {"-s", "KILL", QString::number(m_currentPid)});
        });
    } else if (!m_currentRomId.isEmpty()) {
        // Fall back to IPC kill request
        m_ipcClient->sendKillRequest(0);
    }

    // Reset in-game state
    m_isPlaying = false;
    m_currentRomId.clear();
    m_currentRomPath.clear();
    m_currentPid = -1;

    // Reset UI
    m_previewPlayer->stopPreview();
    m_heroTitle->setText("Select a game to play");
    m_heroSubtitle->setText("Browse your library and launch any title");
    m_soundManager->play(UISoundManager::Select);
}

void MainWindow::goHome()
{
    m_ingameMenu->setVisible(false);
    switchPage(0);
    m_soundManager->play(UISoundManager::Select);
}

void MainWindow::exitHyperVane()
{
    // Kill the emulator first
    if (m_isPlaying) {
        if (m_currentPid > 0) {
            QProcess::startDetached("/bin/kill", {"-s", "TERM", QString::number(m_currentPid)});
        } else if (!m_currentRomId.isEmpty()) {
            m_ipcClient->sendKillRequest(0);
        }
    }

    m_soundManager->play(UISoundManager::Select);
    QApplication::quit();
}
