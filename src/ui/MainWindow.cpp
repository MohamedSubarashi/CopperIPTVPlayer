#include "ui/MainWindow.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QKeySequence>
#include <QLineEdit>
#include <QListView>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QShortcut>
#include <QSplitter>
#include <QStatusBar>
#include <QTextEdit>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include "core/AppSettings.h"
#include "core/AppInfo.h"
#include "core/Logger.h"
#include "database/DatabaseManager.h"
#include "models/Channel.h"
#include "models/Playlist.h"
#include "network/ChannelLogoLoader.h"
#include "network/NetworkManager.h"
#include "player/IPTVPlayer.h"
#include "player/PlaybackController.h"
#include "playlist/PlaylistManager.h"
#include "ui/ControlBar.h"
#include "ui/dialogs/AboutDialog.h"
#include "ui/dialogs/AddPlaylistDialog.h"
#include "ui/dialogs/ChannelInfoDialog.h"
#include "ui/dialogs/PlaylistManagerDialog.h"
#include "ui/dialogs/RestoreDeletedDialog.h"
#include "ui/dialogs/SettingsDialog.h"
#include "ui/models/ChannelFilterProxyModel.h"
#include "ui/models/ChannelListModel.h"
#include "ui/models/GroupListModel.h"
#include "ui/models/PlaylistListModel.h"
#include "ui/widgets/ChannelListWidget.h"
#include "ui/widgets/PlaylistSidebar.h"
#include "ui/widgets/VideoPlayerWidget.h"

namespace {
constexpr int kLoadingTimeoutMs = 20000;
} // namespace

MainWindow::MainWindow(AppSettings *settings, DatabaseManager *database,
                       NetworkManager *network,
                       PlaylistManager *playlistManager,
                       ChannelLogoLoader *logoLoader, QWidget *parent)
    : QMainWindow(parent), m_settings(settings), m_database(database),
      m_network(network), m_playlistManager(playlistManager),
      m_logoLoader(logoLoader)
{
    setWindowTitle(QStringLiteral("Copper IPTV Player"));
    setAcceptDrops(true);
    resize(1280, 800);

    m_player = new IPTVPlayer(this);
    m_playback = new PlaybackController(m_player, m_settings, this);

    m_playlistModel = new PlaylistListModel(this);
    m_groupModel = new GroupListModel(this);
    m_channelModel = new ChannelListModel(this);
    m_channelProxy = new ChannelFilterProxyModel(this);
    m_channelProxy->setSourceModel(m_channelModel);
    m_channelProxy->setDynamicSortFilter(true);

    buildCentralWidget();
    buildMenus();
    buildConnections();
    buildShortcuts();
    restoreWindowState();

    m_sidebar->setPlaylistModel(m_playlistModel);
    m_sidebar->setGroupModel(m_groupModel);
    m_channelWidget->setModels(m_channelModel, m_channelProxy);

    m_player->setVideoOutput(m_videoWidget->videoSurface());
    m_player->setVolume(m_settings->volume());
    m_player->setMuted(m_settings->muted());
    m_controlBar->setVolume(m_settings->volume());
    m_controlBar->setMuted(m_settings->muted());
    m_channelWidget->setCompact(m_settings->compactChannelList());
    m_channelWidget->setAlphabeticalSort(
        m_settings->sortChannelsAlphabetically());
    m_sidebar->setVisible(m_settings->sidebarVisible());
    m_channelWidget->setVisible(m_settings->channelListVisible());
    m_playback->setViews(m_channelWidget->listView(), m_channelProxy);

    m_playlistModel->setPlaylists(m_playlistManager->playlists());
    setInitialPlaylist();
    restoreSelection();
    resumeLastChannel();
}

void MainWindow::startLoadingWatchdog()
{
    if (!m_loadingWatchdog) {
        m_loadingWatchdog = new QTimer(this);
        m_loadingWatchdog->setSingleShot(true);
        m_loadingWatchdog->setInterval(kLoadingTimeoutMs);
        connect(m_loadingWatchdog, &QTimer::timeout, this, [this]() {
            if (m_videoWidget->currentView() != VideoPlayerWidget::View::Loading)
                return;
            m_playback->stop();
            m_videoWidget->showError(
                tr("Playback timed out"),
                tr("The channel took too long to start playing. It may be "
                   "offline or not responding."));
        });
    }
    m_loadingWatchdog->stop();
    m_loadingWatchdog->start();
}

void MainWindow::stopLoadingWatchdog()
{
    if (m_loadingWatchdog)
        m_loadingWatchdog->stop();
}

void MainWindow::buildCentralWidget()
{
    auto *central = new QWidget(this);
    m_mainSplitter = new QSplitter(Qt::Horizontal, central);

    m_sidebar = new PlaylistSidebar(m_mainSplitter);
    m_channelWidget = new ChannelListWidget(m_logoLoader, m_mainSplitter);
    m_videoWidget = new VideoPlayerWidget(m_mainSplitter);

    auto *rightArea = new QWidget(m_mainSplitter);
    m_videoPanel = rightArea;
    auto *rightLayout = new QVBoxLayout(rightArea);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);
    rightLayout->addWidget(m_videoWidget, 1);
    m_controlBar = new ControlBar(rightArea);
    rightLayout->addWidget(m_controlBar);

    m_mainSplitter->addWidget(m_sidebar);
    m_mainSplitter->addWidget(m_channelWidget);
    m_mainSplitter->addWidget(rightArea);
    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 0);
    m_mainSplitter->setStretchFactor(2, 1);
    m_mainSplitter->setCollapsible(0, true);
    m_mainSplitter->setCollapsible(1, true);
    m_mainSplitter->setCollapsible(2, false);

    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_mainSplitter);
    setCentralWidget(central);

    const QList<int> sizes = {220, 300, 760};
    m_mainSplitter->setSizes(sizes);
}

void MainWindow::buildMenus()
{
    auto *playlistMenu = menuBar()->addMenu(tr("&Playlist"));
    auto *addAction = playlistMenu->addAction(tr("&Add Playlist..."));
    addAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+N")));
    connect(addAction, &QAction::triggered, this, &MainWindow::addPlaylist);
    auto *manageAction = playlistMenu->addAction(tr("&Manage Playlists..."));
    connect(manageAction, &QAction::triggered, this,
            &MainWindow::managePlaylists);
    auto *restoreAction =
        playlistMenu->addAction(tr("Restore Removed Channels..."));
    connect(restoreAction, &QAction::triggered, this,
            &MainWindow::restoreRemovedChannels);

    playlistMenu->addSeparator();
    auto *refreshAllAction = playlistMenu->addAction(tr("&Refresh All Remote"));
    connect(refreshAllAction, &QAction::triggered, this, [this]() {
        if (m_playlistManager)
            m_playlistManager->refreshAllRemote();
    });
    auto *exitAction = playlistMenu->addAction(tr("E&xit"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    auto *viewMenu = menuBar()->addMenu(tr("&View"));
    auto *sidebarAction = viewMenu->addAction(tr("Playlists && Groups"));
    sidebarAction->setCheckable(true);
    sidebarAction->setChecked(m_settings->sidebarVisible());
    connect(sidebarAction, &QAction::toggled, this, &MainWindow::toggleSidebar);
    auto *channelListAction = viewMenu->addAction(tr("Channel &List"));
    channelListAction->setCheckable(true);
    channelListAction->setChecked(m_settings->channelListVisible());
    connect(channelListAction, &QAction::toggled, this,
            &MainWindow::toggleChannelList);
    viewMenu->addSeparator();
    auto *fullscreenAction = viewMenu->addAction(tr("&Full Screen"));
    fullscreenAction->setShortcut(QKeySequence(QStringLiteral("F11")));
    connect(fullscreenAction, &QAction::triggered, this,
            &MainWindow::toggleFullscreen);

    auto *goMenu = menuBar()->addMenu(tr("&Go"));
    auto *prevAction = goMenu->addAction(tr("Pre&vious Channel"));
    prevAction->setShortcut(QKeySequence(QStringLiteral("F8")));
    connect(prevAction, &QAction::triggered, this,
            [this]() { m_playback->playPrevious(); });
    auto *nextAction = goMenu->addAction(tr("&Next Channel"));
    nextAction->setShortcut(QKeySequence(QStringLiteral("F9")));
    connect(nextAction, &QAction::triggered, this,
            [this]() { m_playback->playNext(); });
    goMenu->addSeparator();
    auto *focusSearchAction = goMenu->addAction(tr("&Search Channels"));
    focusSearchAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+F")));
    connect(focusSearchAction, &QAction::triggered, this,
            [this]() { m_channelWidget->focusSearch(); });

    auto *toolsMenu = menuBar()->addMenu(tr("&Tools"));
    auto *settingsAction = toolsMenu->addAction(tr("&Settings..."));
    settingsAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+,")));
    connect(settingsAction, &QAction::triggered, this,
            &MainWindow::openSettingsDialog);

    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    auto *aboutAction = helpMenu->addAction(tr("&About Copper IPTV Player"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
}

void MainWindow::buildConnections()
{
    connect(m_sidebar, &PlaylistSidebar::playlistActivated, this,
            &MainWindow::onPlaylistActivated);
    connect(m_sidebar, &PlaylistSidebar::playlistSelectionChanged, this,
            &MainWindow::onPlaylistSelectionChanged);
    connect(m_sidebar, &PlaylistSidebar::groupActivated, this,
            &MainWindow::onGroupActivated);
    connect(m_sidebar, &PlaylistSidebar::removePlaylistRequested, this,
            &MainWindow::onRemovePlaylistRequested);
    connect(m_sidebar, &PlaylistSidebar::refreshPlaylistRequested, this,
            &MainWindow::onRefreshPlaylistRequested);
    connect(m_sidebar, &PlaylistSidebar::addPlaylistRequested, this,
            &MainWindow::addPlaylist);
    connect(m_sidebar, &PlaylistSidebar::managePlaylistsRequested, this,
            &MainWindow::managePlaylists);

    connect(m_channelWidget, &ChannelListWidget::channelClicked, this,
            [this](const Channel &) {
                if (!m_settings || !m_settings->autoplaySelected())
                    return;
                const QModelIndex index =
                    m_channelWidget->listView()->currentIndex();
                if (index.isValid())
                    m_playback->playRow(index.row());
            });
    connect(m_channelWidget, &ChannelListWidget::channelActivated, this,
            [this](const Channel &) {
                const QModelIndex index =
                    m_channelWidget->listView()->currentIndex();
                if (index.isValid())
                    m_playback->playRow(index.row());
            });
    connect(m_channelWidget, &ChannelListWidget::playRequested, this,
            [this](const Channel &) {
                const QModelIndex index =
                    m_channelWidget->listView()->currentIndex();
                if (index.isValid())
                    m_playback->playRow(index.row());
            });
    connect(m_channelWidget, &ChannelListWidget::toggleFavoriteRequested, this,
            [this](const Channel &channel) {
                if (m_playlistManager && m_playlistManager->isFavorite(channel.streamUrl()))
                    removeFavorite(channel);
                else
                    addFavorite(channel);
            });
    connect(m_channelWidget, &ChannelListWidget::copyUrlRequested, this,
            [this](const Channel &channel) { copyToClipboard(channel, true); });
    connect(m_channelWidget, &ChannelListWidget::copyNameRequested, this,
            [this](const Channel &channel) { copyToClipboard(channel, false); });
    connect(m_channelWidget, &ChannelListWidget::infoRequested, this,
            &MainWindow::showChannelInfo);
    connect(m_channelWidget, &ChannelListWidget::deleteRequested, this,
            &MainWindow::onDeleteChannelRequested);
    connect(m_channelWidget, &ChannelListWidget::sortOrderChanged, this,
            [this](bool alphabetical) {
                if (m_settings)
                    m_settings->setSortChannelsAlphabetically(alphabetical);
            });

    connect(m_videoWidget, &VideoPlayerWidget::fullscreenRequested, this,
            &MainWindow::toggleFullscreen);
    connect(m_videoWidget, &VideoPlayerWidget::backRequested, this,
            [this]() { toggleFullscreen(); });
    connect(m_videoWidget, &VideoPlayerWidget::addPlaylistRequested, this,
            &MainWindow::addPlaylist);
    connect(m_controlBar, &ControlBar::playPauseRequested, this,
            [this]() { m_playback->togglePlayPause(); });
    connect(m_controlBar, &ControlBar::stopRequested, this,
            [this]() { stopPlayback(); });
    connect(m_controlBar, &ControlBar::previousRequested, this,
            [this]() { m_playback->playPrevious(); });
    connect(m_controlBar, &ControlBar::nextRequested, this,
            [this]() { m_playback->playNext(); });
    connect(m_controlBar, &ControlBar::volumeChanged, this,
            [this](int volume) {
                m_player->setVolume(volume);
                m_settings->setVolume(volume);
            });
    connect(m_controlBar, &ControlBar::muteToggled, this, [this](bool muted) {
        m_player->setMuted(muted);
        m_settings->setMuted(muted);
        m_controlBar->setMuted(muted);
    });
    connect(m_controlBar, &ControlBar::fullscreenRequested, this,
            &MainWindow::toggleFullscreen);

    connect(m_playback, &PlaybackController::statusMessage, this,
            [this](const QString &message) {
                statusBar()->showMessage(message, 4000);
            });
    connect(m_playback, &PlaybackController::nowPlaying, this,
            [this](const QString &name) {
                setWindowTitle(tr("%1 \u2014 Copper IPTV Player").arg(name));
                setVideoPanelVisible(true);
                m_videoWidget->showLoading(name);
                startLoadingWatchdog();
                m_settings->setLastChannelUrl(m_playback->currentUrl().toString());
            });
    connect(m_player, &IPTVPlayer::playbackStarted, this, [this]() {
        stopLoadingWatchdog();
        m_videoWidget->showVideo();
    });
    connect(m_playback, &PlaybackController::playbackError, this,
            [this](const QString &message) {
                stopLoadingWatchdog();
                statusBar()->showMessage(message, 8000);
                if (m_videoWidget->currentView() ==
                    VideoPlayerWidget::View::Loading)
                    m_videoWidget->showError(tr("Playback failed"), message);
            });
    connect(m_playback, &PlaybackController::playStateChanged, this,
            [this](bool playing) { m_controlBar->setPlaying(playing); });

    if (m_playlistManager) {
        connect(m_playlistManager, &PlaylistManager::playlistsChanged, this,
                [this]() { m_playlistModel->setPlaylists(m_playlistManager->playlists()); });
        connect(m_playlistManager, &PlaylistManager::playlistLoaded, this,
                &MainWindow::onPlaylistLoaded);
        connect(m_playlistManager, &PlaylistManager::playlistLoadFailed, this,
                &MainWindow::onPlaylistLoadFailed);
        connect(m_playlistManager, &PlaylistManager::favoritesChanged, this,
                &MainWindow::onFavoritesChanged);
        connect(m_playlistManager, &PlaylistManager::playlistStatus, this,
                [this](const QString &message) {
                    statusBar()->showMessage(message, 4000);
                });
    }
}

void MainWindow::buildShortcuts()
{
    auto textFocused = []() {
        const QWidget *focus = QApplication::focusWidget();
        return focus && (qobject_cast<const QLineEdit *>(focus) ||
                         qobject_cast<const QTextEdit *>(focus));
    };

    auto *spaceShortcut = new QShortcut(QKeySequence(QStringLiteral("Space")), this);
    connect(spaceShortcut, &QShortcut::activated, this, [=, this]() {
        if (textFocused())
            return;
        m_playback->togglePlayPause();
    });

    auto *fShortcut = new QShortcut(QKeySequence(QStringLiteral("F")), this);
    connect(fShortcut, &QShortcut::activated, this, [=, this]() {
        if (textFocused())
            return;
        toggleFullscreen();
    });

    auto *mShortcut = new QShortcut(QKeySequence(QStringLiteral("M")), this);
    connect(mShortcut, &QShortcut::activated, this, [=, this]() {
        if (textFocused())
            return;
        toggleMute();
    });

    auto *stopShortcut =
        new QShortcut(QKeySequence(QStringLiteral("Ctrl+S")), this);
    connect(stopShortcut, &QShortcut::activated, this, [=, this]() {
        if (textFocused())
            return;
        stopPlayback();
    });

    auto *prevShortcut =
        new QShortcut(QKeySequence(QStringLiteral("Ctrl+Left")), this);
    connect(prevShortcut, &QShortcut::activated, this, [=, this]() {
        if (textFocused())
            return;
        m_playback->playPrevious();
    });

    auto *nextShortcut =
        new QShortcut(QKeySequence(QStringLiteral("Ctrl+Right")), this);
    connect(nextShortcut, &QShortcut::activated, this, [=, this]() {
        if (textFocused())
            return;
        m_playback->playNext();
    });

    auto *escShortcut = new QShortcut(QKeySequence(QStringLiteral("Esc")), this);
    connect(escShortcut, &QShortcut::activated, this, [=, this]() {
        if (isFullScreen())
            toggleFullscreen();
    });

    auto *backspaceShortcut =
        new QShortcut(QKeySequence(QStringLiteral("Backspace")), this);
    connect(backspaceShortcut, &QShortcut::activated, this, [=, this]() {
        if (textFocused() || !isFullScreen())
            return;
        toggleFullscreen();
    });
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_settings && m_settings->rememberWindowState()) {
        m_settings->setWindowGeometry(saveGeometry());
        m_settings->setWindowState(saveState());
    }
    if (m_settings && m_mainSplitter)
        m_settings->setSplitterState(m_mainSplitter->saveState());
    m_playback->stop();
    QMainWindow::closeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (!event->mimeData() || !event->mimeData()->hasUrls())
        return;
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        if (url.isLocalFile() &&
            (url.toLocalFile().endsWith(QStringLiteral(".m3u"),
                                        Qt::CaseInsensitive) ||
             url.toLocalFile().endsWith(QStringLiteral(".m3u8"),
                                        Qt::CaseInsensitive))) {
            event->acceptProposedAction();
            return;
        }
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (!event->mimeData() || !event->mimeData()->hasUrls() ||
        !m_playlistManager)
        return;
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        if (!url.isLocalFile())
            continue;
        const QString path = url.toLocalFile();
        if (path.endsWith(QStringLiteral(".m3u"), Qt::CaseInsensitive) ||
            path.endsWith(QStringLiteral(".m3u8"), Qt::CaseInsensitive)) {
            QString error;
            const int id = m_playlistManager->addLocalPlaylist(path, &error);
            if (id < 0) {
                statusBar()->showMessage(
                    tr("Could not add '%1': %2").arg(QFileInfo(path).fileName(), error),
                    6000);
            } else {
                activatePlaylist(id);
            }
        }
    }
    event->acceptProposedAction();
}

void MainWindow::onPlaylistActivated(int id)
{
    Q_UNUSED(id)
    // Playlist loads are driven by playlistSelectionChanged (click and arrow
    // keys both select). Keeping both connected keeps the UI responsive.
}

void MainWindow::onPlaylistSelectionChanged(int id)
{
    if (id != m_activePlaylistId) {
        m_activePlaylistId = id;
        if (m_channelProxy) {
            m_channelProxy->setGroup(QString());
            m_sidebar->selectGroup(QString());
        }
        reloadActivePlaylist();
    }
}

void MainWindow::onGroupActivated(const QString &groupKey)
{
    if (m_channelProxy)
        m_channelProxy->setGroup(groupKey);
}

void MainWindow::onPlaylistLoaded(int playlistId, int channelCount)
{
    if (playlistId == m_activePlaylistId)
        reloadActivePlaylist();

    const Playlist p = m_playlistManager->playlistById(playlistId);
    if (p.id >= 0 && p.enabled) {
        statusBar()->showMessage(
            tr("Loaded %1 channels from '%2'.").arg(channelCount).arg(p.name),
            5000);
    }
    m_playlistModel->setPlaylists(m_playlistManager->playlists());
}

void MainWindow::onPlaylistLoadFailed(int playlistId, const QString &message)
{
    const Playlist p =
        m_playlistManager ? m_playlistManager->playlistById(playlistId) : Playlist();
    statusBar()->showMessage(
        tr("Failed to load playlist '%1': %2")
            .arg(p.id >= 0 ? p.name : QString::number(playlistId), message),
        6000);
    if (playlistId == m_activePlaylistId && m_channelModel->rowCount() == 0)
        m_videoWidget->showError(tr("Playlist load failed"), message);
}

void MainWindow::onFavoritesChanged()
{
    if (m_activePlaylistId >= 0)
        reloadActivePlaylist();
}

void MainWindow::reloadActivePlaylist()
{
    if (m_activePlaylistId < 0 || !m_playlistManager)
        return;

    const QVector<Channel> channels =
        m_playlistManager->channels(m_activePlaylistId);
    applyActivePlaylistChannels(channels);
}

void MainWindow::applyActivePlaylistChannels(const QVector<Channel> &channels)
{
    QVector<Channel> withFavorites = channels;
    if (m_playlistManager) {
        for (Channel &channel : withFavorites)
            channel.isFavorite =
                m_playlistManager->isFavorite(channel.streamUrl());
    }

    m_channelModel->setChannels(withFavorites);
    m_channelProxy->refilter();

    const QSet<QString> favorites = m_playlistManager
                                        ? m_playlistManager->favoriteUrls()
                                        : QSet<QString>();
    m_groupModel->setData(withFavorites, favorites);
}

void MainWindow::activatePlaylist(int id)
{
    if (!m_playlistManager || m_playlistManager->playlistById(id).id < 0)
        return;
    const bool switched = (id != m_activePlaylistId);
    m_activePlaylistId = id;
    if (switched) {
        m_channelProxy->setGroup(QString());
        m_sidebar->selectGroup(QString());
    }
    m_sidebar->selectPlaylistById(id);
    if (m_settings)
        m_settings->setLastPlaylistId(id);
    reloadActivePlaylist();
}

void MainWindow::removePlaylist(int id)
{
    if (!m_playlistManager || m_playlistManager->playlistById(id).id < 0)
        return;

    if (m_settings && m_settings->confirmPlaylistRemoval()) {
        const Playlist p = m_playlistManager->playlistById(id);
        const auto answer = QMessageBox::question(
            this, tr("Remove Playlist"),
            tr("Remove playlist '%1'? Its cached channels will be deleted.")
                .arg(p.name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    const bool wasActive = (id == m_activePlaylistId);
    if (!m_playlistManager->removePlaylist(id))
        return;

    if (wasActive) {
        const int fallback = findFallbackPlaylistId();
        if (fallback >= 0) {
            activatePlaylist(fallback);
        } else {
            m_activePlaylistId = -1;
            if (m_settings)
                m_settings->setLastPlaylistId(-1);
            applyActivePlaylistChannels({});
            m_videoWidget->showWelcome();
            setVideoPanelVisible(true);
        }
    }
    statusBar()->showMessage(tr("Playlist removed."), 3000);
}

int MainWindow::findFallbackPlaylistId() const
{
    if (!m_playlistManager)
        return -1;
    const QVector<Playlist> playlists = m_playlistManager->playlists();
    for (const Playlist &p : playlists) {
        if (p.enabled)
            return p.id;
    }
    return playlists.isEmpty() ? -1 : playlists.first().id;
}

void MainWindow::onRemovePlaylistRequested(int id)
{
    removePlaylist(id);
}

void MainWindow::onRefreshPlaylistRequested(int id)
{
    if (m_playlistManager)
        m_playlistManager->refreshPlaylist(id);
}

void MainWindow::addFavorite(const Channel &channel)
{
    if (!m_playlistManager)
        return;
    m_playlistManager->addFavorite(channel);
    statusBar()->showMessage(tr("Added to favorites: %1").arg(channel.name), 3000);
}

void MainWindow::removeFavorite(const Channel &channel)
{
    if (!m_playlistManager)
        return;
    m_playlistManager->removeFavorite(channel.streamUrl());
    statusBar()->showMessage(tr("Removed from favorites: %1").arg(channel.name),
                             3000);
}

void MainWindow::toggleFullscreen()
{
    if (isFullScreen()) {
        showNormal();
        if (m_videoWidget)
            m_videoWidget->setFullscreenMode(false);
        const bool sidebarVisible =
            m_settings ? m_settings->sidebarVisible() : true;
        const bool channelsVisible =
            m_settings ? m_settings->channelListVisible() : true;
        m_sidebar->setVisible(sidebarVisible);
        m_channelWidget->setVisible(channelsVisible);
        const bool panelNeeded =
            m_playlistManager && m_playlistManager->playlists().isEmpty();
        const bool hasChannel =
            m_playback && m_playback->currentChannel().isValid();
        setVideoPanelVisible(panelNeeded || hasChannel);
    } else {
        showFullScreen();
        m_sidebar->setVisible(false);
        m_channelWidget->setVisible(false);
        setVideoPanelVisible(true);
        if (m_videoWidget)
            m_videoWidget->setFullscreenMode(true);
    }
}

void MainWindow::setVideoPanelVisible(bool visible)
{
    if (m_videoPanel)
        m_videoPanel->setVisible(visible);
}

void MainWindow::stopPlayback()
{
    stopLoadingWatchdog();
    m_playback->stop();
    m_videoWidget->showWelcome();
    setVideoPanelVisible(false);
}

void MainWindow::toggleMute()
{
    if (!m_settings)
        return;
    const bool muted = !m_settings->muted();
    m_player->setMuted(muted);
    m_settings->setMuted(muted);
    m_controlBar->setMuted(muted);
}

void MainWindow::toggleSidebar(bool visible)
{
    if (m_settings)
        m_settings->setSidebarVisible(visible);
    if (m_sidebar)
        m_sidebar->setVisible(visible);
}

void MainWindow::toggleChannelList(bool visible)
{
    if (m_settings)
        m_settings->setChannelListVisible(visible);
    if (m_channelWidget)
        m_channelWidget->setVisible(visible);
}

void MainWindow::copyToClipboard(const Channel &channel, bool url)
{
    const QString text = url ? ChannelUtils::maskedUrl(channel.streamUrl())
                             : channel.name;
    QGuiApplication::clipboard()->setText(text);
    statusBar()->showMessage(url ? tr("Stream URL copied to clipboard.")
                                 : tr("Channel name copied to clipboard."),
                             3000);
}

void MainWindow::showChannelInfo(const Channel &channel)
{
    const bool isFavorite = m_playlistManager &&
                            m_playlistManager->isFavorite(channel.streamUrl());
    ChannelInfoDialog dialog(channel, isFavorite, m_logoLoader, this);
    dialog.exec();
}

void MainWindow::addPlaylist()
{
    if (!m_playlistManager)
        return;

    AddPlaylistDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    QString error;
    int id = -1;
    if (dialog.sourceType() == Playlist::SourceType::Local) {
        id = m_playlistManager->addLocalPlaylist(dialog.sourceValue(), &error);
    } else {
        id = m_playlistManager->addRemotePlaylist(dialog.sourceValue(), &error);
    }
    if (id < 0) {
        QMessageBox::warning(this, tr("Add Playlist"),
                             tr("Unable to add the playlist.\n%1")
                                 .arg(error.isEmpty() ? tr("Unknown error")
                                                      : error));
        return;
    }

    const QString desired = dialog.playlistName();
    if (!desired.isEmpty())
        m_playlistManager->renamePlaylist(id, desired);

    m_playlistModel->setPlaylists(m_playlistManager->playlists());

    // Select the new playlist so its channels start loading right away.
    activatePlaylist(id);
}

void MainWindow::managePlaylists()
{
    if (!m_playlistManager)
        return;
    PlaylistManagerDialog dialog(m_playlistManager, m_settings, this);
    dialog.exec();
    m_playlistModel->setPlaylists(m_playlistManager->playlists());

    const int added = dialog.lastAddedPlaylistId();
    const int removed = dialog.lastRemovedPlaylistId();
    if (added >= 0 && m_playlistManager->playlistById(added).id >= 0) {
        activatePlaylist(added);
        return;
    }
    if (removed >= 0 && removed == m_activePlaylistId) {
        const int fallback = findFallbackPlaylistId();
        if (fallback >= 0) {
            activatePlaylist(fallback);
        } else {
            m_activePlaylistId = -1;
            if (m_settings)
                m_settings->setLastPlaylistId(-1);
            applyActivePlaylistChannels({});
            m_videoWidget->showWelcome();
            setVideoPanelVisible(true);
        }
    }
}

void MainWindow::restoreRemovedChannels()
{
    if (!m_playlistManager)
        return;
    QVector<Channel> blocked = m_activePlaylistId >= 0
                                   ? m_playlistManager->blockedChannels(
                                         m_activePlaylistId)
                                   : QVector<Channel>();
    if (blocked.isEmpty()) {
        QMessageBox::information(
            this, tr("Restore Removed Channels"),
            tr("No channels have been removed from the current playlist."));
        return;
    }

    RestoreDeletedDialog dialog(blocked, this);
    connect(&dialog, &RestoreDeletedDialog::restoreRequested, this,
            [this, &dialog](const QVector<Channel> &channels) {
                for (const Channel &channel : channels)
                    m_playlistManager->restoreChannel(channel);
                if (m_activePlaylistId >= 0)
                    reloadActivePlaylist();
                dialog.setChannels(
                    m_playlistManager->blockedChannels(m_activePlaylistId));
            });
    dialog.exec();
}

void MainWindow::onDeleteChannelRequested(const Channel &channel)
{
    if (!m_playlistManager || m_activePlaylistId < 0)
        return;

    if (QMessageBox::warning(
            this, tr("Delete Channel"),
            tr("Delete \"%1\" from this playlist?\n\nThe channel stays "
               "removed even after the playlist is refreshed. You can restore "
               "it later via Playlist > Restore Removed Channels.")
                .arg(channel.name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) !=
        QMessageBox::Yes)
        return;

    if (m_playback && m_playback->currentChannel().streamUrl() ==
                          channel.streamUrl())
        stopPlayback();
    if (m_playlistManager->removeChannel(channel) && m_activePlaylistId >= 0)
        reloadActivePlaylist();
}

void MainWindow::openSettingsDialog()
{
    if (!m_settings)
        return;
    SettingsDialog dialog(m_settings, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    if (m_network)
        m_network->setUserAgent(m_settings->userAgent());
    if (m_playlistManager)
        m_playlistManager->setAutoRefreshMinutes(m_settings->autoRefreshMinutes());

    m_player->setVolume(m_settings->volume());
    m_controlBar->setVolume(m_settings->volume());
    m_channelWidget->setCompact(m_settings->compactChannelList());
    toggleSidebar(m_settings->sidebarVisible());
    toggleChannelList(m_settings->channelListVisible());
}

void MainWindow::restoreWindowState()
{
    if (!m_settings)
        return;
    if (m_settings->rememberWindowState()) {
        const QByteArray geometry = m_settings->windowGeometry();
        if (!geometry.isEmpty())
            restoreGeometry(geometry);
        const QByteArray state = m_settings->windowState();
        if (!state.isEmpty())
            restoreState(state);
    }
    const QByteArray splitter = m_settings->splitterState();
    if (m_mainSplitter && !splitter.isEmpty())
        m_mainSplitter->restoreState(splitter);
}

void MainWindow::setInitialPlaylist()
{
    if (!m_playlistManager)
        return;

    const QVector<Playlist> playlists = m_playlistManager->playlists();
    if (playlists.isEmpty()) {
        m_activePlaylistId = -1;
        m_videoWidget->showWelcome();
        setVideoPanelVisible(true);
        return;
    }

    setVideoPanelVisible(false);

    int initialId = m_settings ? m_settings->lastPlaylistId() : -1;
    if (initialId >= 0 && m_playlistManager->playlistById(initialId).id < 0)
        initialId = -1;

    if (initialId < 0)
        initialId = findFallbackPlaylistId();
    if (initialId < 0)
        initialId = playlists.first().id;

    m_activePlaylistId = initialId;
    m_sidebar->selectPlaylistById(initialId);
    reloadActivePlaylist();
}

void MainWindow::restoreSelection()
{
    if (!m_settings || !m_settings->rememberLastChannel())
        return;
    const QString lastUrl = m_settings->lastChannelUrl();
    if (lastUrl.isEmpty())
        return;

    for (int row = 0; row < m_channelProxy->rowCount(); ++row) {
        const Channel channel =
            m_channelProxy->data(m_channelProxy->index(row, 0),
                                 ChannelListModel::ChannelRole)
                .value<Channel>();
        if (!channel.streamUrl().isEmpty() && channel.streamUrl() == lastUrl) {
            m_channelWidget->listView()->setCurrentIndex(
                m_channelProxy->index(row, 0));
            break;
        }
    }
}

void MainWindow::resumeLastChannel()
{
    if (!m_settings || !m_settings->resumeOnStart() || m_activePlaylistId < 0)
        return;

    const Channel channel = m_playback->currentChannel();
    if (channel.isValid())
        return;

    const QString lastUrl = m_settings->lastChannelUrl();
    if (lastUrl.isEmpty())
        return;

    const QVector<Channel> channels =
        m_playlistManager->channels(m_activePlaylistId);
    for (const Channel &candidate : channels) {
        if (candidate.streamUrl() == lastUrl) {
            m_playback->selectAndPlay(candidate);
            break;
        }
    }
}

void MainWindow::showAbout()
{
    AboutDialog dialog(this);
    dialog.exec();
}