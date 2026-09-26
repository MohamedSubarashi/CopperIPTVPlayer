#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

#include "models/Channel.h"
#include "models/Playlist.h"

class QSqlDatabase;

// SQLite-backed persistence for playlists, cached channel snapshots, and
// favorites. Settings/window state live in QSettings (see AppSettings).
// All failures are reported via lastError() and logged; the application keeps
// running even if persistence is unavailable.
class DatabaseManager : public QObject
{
    Q_OBJECT
public:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager() override;

    bool open();
    void close();
    bool isOpen() const;
    QString databasePath() const;
    QString lastError() const;

    // Playlists
    QVector<Playlist> loadPlaylists();
    int addPlaylist(const Playlist &playlist);
    bool updatePlaylist(const Playlist &playlist);
    bool removePlaylist(int playlistId);
    bool updateChannelCount(int playlistId, int count);
    bool updateLastRefresh(int playlistId, const QDateTime &when);

    // Channel snapshot
    bool replaceChannels(int playlistId, const QVector<Channel> &channels);
    QVector<Channel> loadChannels(int playlistId);
    bool insertChannel(const Channel &channel);
    bool deleteChannel(int playlistId, const QString &url);

    // Removed/blocked channels (per playlist, keyed by URL so they survive
    // playlist refreshes; restore re-inserts them into the snapshot).
    bool blockChannel(const Channel &channel);
    bool unblockChannel(int playlistId, const QString &url);
    QVector<Channel> blockedChannels(int playlistId);
    QSet<QString> blockedUrls(int playlistId);

    // Favorites (global, keyed by URL so they survive playlist edits)
    QSet<QString> favoriteUrls();
    QVector<Channel> favoriteChannels();
    bool isFavorite(const QString &url) const;
    bool addFavorite(const Channel &channel);
    bool removeFavorite(const QString &url);

signals:
    void error(const QString &message);

private:
    bool createSchema();
    bool runMigrations();
    void reportError(const QString &where, const QString &message);

    QSqlDatabase *m_db = nullptr;
    QString m_connectionName;
    QString m_path;
    QString m_lastError;
    mutable QSet<QString> m_favoriteCache;
    mutable bool m_favoritesLoaded = false;
};