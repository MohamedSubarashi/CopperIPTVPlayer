#pragma once

#include <QFutureWatcher>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

#include "models/Channel.h"
#include "models/Playlist.h"
#include "network/PlaylistDownloader.h"

class AppSettings;
class DatabaseManager;
class NetworkManager;
class QTimer;

// Owns playlist metadata, coordinates async download/parse/storage, and
// maintains the favorites set. Data source of truth for channels is the
// SQLite snapshot; refreshes replace it only on success.
class PlaylistManager : public QObject
{
    Q_OBJECT
public:
    PlaylistManager(DatabaseManager *database, NetworkManager *network,
                    AppSettings *settings, QObject *parent = nullptr);
    ~PlaylistManager() override;

    // Loads playlists + favorites from the database (startup path).
    void loadFromDatabase();

    QVector<Playlist> playlists() const { return m_playlists; }
    Playlist playlistById(int id) const;
    int indexOfPlaylist(int id) const;

    // Returns the channel snapshot for a playlist (cached).
    const QVector<Channel> &channels(int playlistId);

    // Adds a local .m3u/.m3u8 playlist. Returns new id, or -1 + error message.
    int addLocalPlaylist(const QString &filePath, QString *errorOut = nullptr);
    // Adds a remote playlist (async fetch begins immediately).
    int addRemotePlaylist(const QString &url, QString *errorOut = nullptr);

    bool removePlaylist(int id);
    bool renamePlaylist(int id, const QString &newName);
    bool setPlaylistEnabled(int id, bool enabled);

    void refreshPlaylist(int id);
    void refreshAllRemote();

    // Favorites (persisted through DatabaseManager).
    const QSet<QString> &favoriteUrls() const { return m_favoriteUrls; }
    QVector<Channel> favoriteChannels() const;
    bool isFavorite(const QString &url) const { return m_favoriteUrls.contains(url); }
    void addFavorite(const Channel &channel);
    void removeFavorite(const QString &url);

    // Removed/blocked channels. Removal is durable across refreshes; restore
    // puts the channel back into the snapshot.
    bool removeChannel(const Channel &channel);
    bool restoreChannel(const Channel &channel);
    QVector<Channel> blockedChannels(int playlistId) const;

    void setAutoRefreshMinutes(int minutes);
    bool isRefreshing(int id) const { return m_refreshing.contains(id); }

signals:
    void playlistsChanged();
    void channelsChanged(int playlistId);
    void playlistStatus(const QString &message);
    void playlistLoaded(int playlistId, int channelCount);
    void playlistLoadFailed(int playlistId, const QString &message);
    void favoritesChanged();
    void error(const QString &message);

private:
    void startDownload(int playlistId, const QUrl &url);
    void onDownloadFinished(int token, const PlaylistDownloader::Result &result);
    void startParse(int playlistId, const QByteArray &data, const QString &origin);
    void runAutoRefresh();
    void applyFavoriteFlags(QVector<Channel> *channels) const;

    DatabaseManager *m_database = nullptr;
    NetworkManager *m_network = nullptr;
    AppSettings *m_settings = nullptr;
    PlaylistDownloader *m_downloader = nullptr;
    QTimer *m_autoRefreshTimer = nullptr;

    QVector<Playlist> m_playlists;
    QHash<int, QVector<Channel>> m_channelCache;
    QSet<QString> m_favoriteUrls;
    QSet<int> m_refreshing;
    QVector<QObject *> m_pendingParses;
};