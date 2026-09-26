#pragma once

#include <QMainWindow>

#include <QUrl>

#include "models/Channel.h"
#include "models/Playlist.h"

class AppSettings;
class ChannelFilterProxyModel;
class ChannelListModel;
class ChannelListWidget;
class ChannelLogoLoader;
class ControlBar;
class DatabaseManager;
class GroupListModel;
class IPTVPlayer;
class NetworkManager;
class PlaybackController;
class PlaylistListModel;
class PlaylistManager;
class PlaylistSidebar;
class QSplitter;
class QTimer;
class VideoPlayerWidget;

// Main window: sidebar (playlists + groups) | channel list | video + controls.
// Owns the view layer, wires services together, and persists window state.
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(AppSettings *settings, DatabaseManager *database,
                        NetworkManager *network, PlaylistManager *playlistManager,
                        ChannelLogoLoader *logoLoader, QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void onPlaylistActivated(int id);
    void onPlaylistSelectionChanged(int id);
    void onGroupActivated(const QString &groupKey);
    void onPlaylistLoaded(int playlistId, int channelCount);
    void onPlaylistLoadFailed(int playlistId, const QString &message);
    void onFavoritesChanged();
    void onRemovePlaylistRequested(int id);
    void onRefreshPlaylistRequested(int id);

private:
    void buildMenus();
    void buildCentralWidget();
    void buildConnections();
    void buildShortcuts();

    void reloadActivePlaylist();
    void applyActivePlaylistChannels(const QVector<Channel> &channels);
    void activatePlaylist(int id);
    void removePlaylist(int id);
    int findFallbackPlaylistId() const;
    void addFavorite(const Channel &channel);
    void removeFavorite(const Channel &channel);

    void addPlaylist();
    void managePlaylists();
    void openSettingsDialog();
    void restoreRemovedChannels();
    void onDeleteChannelRequested(const Channel &channel);
    void toggleFullscreen();
    void toggleSidebar(bool visible);
    void toggleChannelList(bool visible);
    void copyToClipboard(const Channel &channel, bool url);
    void showChannelInfo(const Channel &channel);
    void showAbout();

    void restoreWindowState();
    void restoreSelection();
    void resumeLastChannel();
    void setInitialPlaylist();

    void startLoadingWatchdog();
    void stopLoadingWatchdog();
    void setVideoPanelVisible(bool visible);
    void stopPlayback();
    void toggleMute();

    AppSettings *m_settings = nullptr;
    DatabaseManager *m_database = nullptr;
    NetworkManager *m_network = nullptr;
    PlaylistManager *m_playlistManager = nullptr;
    ChannelLogoLoader *m_logoLoader = nullptr;

    IPTVPlayer *m_player = nullptr;
    PlaybackController *m_playback = nullptr;

    PlaylistListModel *m_playlistModel = nullptr;
    GroupListModel *m_groupModel = nullptr;
    ChannelListModel *m_channelModel = nullptr;
    ChannelFilterProxyModel *m_channelProxy = nullptr;

    PlaylistSidebar *m_sidebar = nullptr;
    ChannelListWidget *m_channelWidget = nullptr;
    VideoPlayerWidget *m_videoWidget = nullptr;
    ControlBar *m_controlBar = nullptr;
    QSplitter *m_mainSplitter = nullptr;
    QWidget *m_videoPanel = nullptr;

    int m_activePlaylistId = -1;
    QTimer *m_loadingWatchdog = nullptr;
};