#include "playlist/PlaylistManager.h"

#include <algorithm>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFuture>
#include <QStringDecoder>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent>

#include "core/AppSettings.h"
#include "core/Constants.h"
#include "core/Logger.h"
#include "database/DatabaseManager.h"
#include "network/NetworkManager.h"
#include "playlist/M3UParser.h"

namespace {

// Decodes playlist bytes to text, tolerating a UTF-8 BOM and falling back to
// Latin-1 when the payload is not valid UTF-8.
QString decodePayload(const QByteArray &bytes)
{
    QStringDecoder utf8(QStringDecoder::Utf8);
    QString text = utf8.decode(bytes);
    if (utf8.hasError()) {
        QStringDecoder latin1(QStringDecoder::Latin1);
        text = latin1.decode(bytes);
    }
    if (text.startsWith(QChar(0xFEFF)))
        text.remove(0, 1);
    return text;
}

} // namespace

PlaylistManager::PlaylistManager(DatabaseManager *database,
                                 NetworkManager *network, AppSettings *settings,
                                 QObject *parent)
    : QObject(parent),
      m_database(database),
      m_network(network),
      m_settings(settings),
      m_downloader(new PlaylistDownloader(network, this)),
      m_autoRefreshTimer(new QTimer(this))
{
    m_autoRefreshTimer->setSingleShot(true);
    connect(m_autoRefreshTimer, &QTimer::timeout, this,
            &PlaylistManager::runAutoRefresh);

    connect(m_downloader, &PlaylistDownloader::finished, this,
            &PlaylistManager::onDownloadFinished);
}

PlaylistManager::~PlaylistManager() = default;

void PlaylistManager::loadFromDatabase()
{
    m_playlists = m_database->loadPlaylists();
    m_favoriteUrls = m_database->favoriteUrls();
    m_channelCache.clear();
    m_refreshing.clear();
    emit playlistsChanged();
    qInfo("Playlists loaded: %lld", static_cast<long long>(m_playlists.size()));
}

Playlist PlaylistManager::playlistById(int id) const
{
    for (const Playlist &p : m_playlists) {
        if (p.id == id)
            return p;
    }
    return Playlist();
}

int PlaylistManager::indexOfPlaylist(int id) const
{
    for (int i = 0; i < m_playlists.size(); ++i) {
        if (m_playlists.at(i).id == id)
            return i;
    }
    return -1;
}

const QVector<Channel> &PlaylistManager::channels(int playlistId)
{
    auto it = m_channelCache.find(playlistId);
    if (it != m_channelCache.end())
        return it.value();

    QVector<Channel> loaded = m_database->loadChannels(playlistId);
    const QSet<QString> blocked = m_database->blockedUrls(playlistId);
    if (!blocked.isEmpty()) {
        loaded.erase(std::remove_if(loaded.begin(), loaded.end(),
                                    [&blocked](const Channel &c) {
                                        return blocked.contains(c.streamUrl());
                                    }),
                     loaded.end());
    }
    applyFavoriteFlags(&loaded);
    auto &stored = m_channelCache[playlistId];
    stored = std::move(loaded);
    return stored;
}

int PlaylistManager::addLocalPlaylist(const QString &filePath, QString *errorOut)
{
    const QFileInfo info(filePath);
    const QString suffix = info.suffix().toLower();
    if (!info.exists()) {
        if (errorOut)
            *errorOut = QStringLiteral("The file does not exist.");
        return -1;
    }
    if (suffix != QStringLiteral("m3u") && suffix != QStringLiteral("m3u8")) {
        if (errorOut)
            *errorOut = QStringLiteral("Only .m3u and .m3u8 playlists are supported.");
        return -1;
    }

    QFile file(info.canonicalFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorOut)
            *errorOut = QStringLiteral("Unable to read the file: %1").arg(file.errorString());
        qWarning() << "[playlist] cannot read local file" << info.canonicalFilePath();
        return -1;
    }

    Playlist playlist;
    playlist.name = QFileInfo(info.fileName()).completeBaseName();
    playlist.type = Playlist::SourceType::Local;
    playlist.source = QDir::toNativeSeparators(info.canonicalFilePath());
    playlist.lastRefresh = QDateTime::currentDateTime();
    const int id = m_database->addPlaylist(playlist);
    if (id < 0) {
        if (errorOut)
            *errorOut = m_database->lastError();
        return -1;
    }

    playlist.id = id;
    m_playlists.append(playlist);
    emit playlistsChanged();

    const QByteArray bytes = file.readAll();
    startParse(id, bytes, playlist.source);
    return id;
}

int PlaylistManager::addRemotePlaylist(const QString &url, QString *errorOut)
{
    const QUrl parsed(url);
    if (!(parsed.scheme() == QStringLiteral("http") ||
          parsed.scheme() == QStringLiteral("https")) ||
        parsed.host().isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Please enter a valid http(s) playlist URL.");
        return -1;
    }

    Playlist playlist;
    playlist.name = parsed.host();
    playlist.type = Playlist::SourceType::Remote;
    playlist.source = parsed.toString();
    const int id = m_database->addPlaylist(playlist);
    if (id < 0) {
        if (errorOut)
            *errorOut = m_database->lastError();
        return -1;
    }

    playlist.id = id;
    m_playlists.append(playlist);
    emit playlistsChanged();

    startDownload(id, parsed);
    return id;
}

bool PlaylistManager::removePlaylist(int id)
{
    const int index = indexOfPlaylist(id);
    if (index < 0)
        return false;

    if (!m_database->removePlaylist(id))
        return false;

    m_playlists.removeAt(index);
    m_channelCache.remove(id);
    m_refreshing.remove(id);
    emit playlistsChanged();
    emit channelsChanged(id);
    return true;
}

bool PlaylistManager::renamePlaylist(int id, const QString &newName)
{
    const int index = indexOfPlaylist(id);
    if (index < 0 || newName.isEmpty())
        return false;

    if (m_playlists[index].name == newName)
        return true;

    m_playlists[index].name = newName;
    if (!m_database->updatePlaylist(m_playlists[index])) {
        emit error(tr("Could not save the playlist name: %1")
                       .arg(m_database->lastError()));
        return false;
    }
    emit playlistsChanged();
    return true;
}

bool PlaylistManager::setPlaylistEnabled(int id, bool enabled)
{
    const int index = indexOfPlaylist(id);
    if (index < 0)
        return false;
    m_playlists[index].enabled = enabled;
    if (!m_database->updatePlaylist(m_playlists[index])) {
        emit error(m_database->lastError());
        return false;
    }
    emit playlistsChanged();
    return true;
}

void PlaylistManager::refreshPlaylist(int id)
{
    const Playlist p = playlistById(id);
    if (p.id < 0 || !p.enabled)
        return;

    if (m_refreshing.contains(id)) {
        emit playlistStatus(tr("Playlist is already being refreshed."));
        return;
    }

    if (p.isRemote()) {
        startDownload(id, QUrl(p.source));
    } else {
        QFile file(QDir::fromNativeSeparators(p.source));
        if (!file.open(QIODevice::ReadOnly)) {
            emit playlistLoadFailed(id, tr("Unable to read the playlist file."));
            return;
        }
        const QByteArray bytes = file.readAll();
        startParse(id, bytes, p.source);
    }
}

void PlaylistManager::refreshAllRemote()
{
    for (const Playlist &p : std::as_const(m_playlists)) {
        if (p.isRemote() && p.enabled)
            refreshPlaylist(p.id);
    }
}

void PlaylistManager::startDownload(int playlistId, const QUrl &url)
{
    if (m_refreshing.contains(playlistId))
        return;
    m_refreshing.insert(playlistId);
    emit playlistStatus(tr("Downloading playlist..."));
    qInfo() << "[playlist] downloading" << url.toDisplayString().left(120);
    m_downloader->download(url, m_settings->downloadTimeoutMs(),
                           m_settings->ignoreTlsErrors(), playlistId);
}

void PlaylistManager::onDownloadFinished(int token,
                                         const PlaylistDownloader::Result &result)
{
    m_refreshing.remove(token);
    if (!result.ok) {
        qWarning().noquote()
            << "[playlist] download failed for" << result.url.left(120)
            << result.detail;
        emit playlistStatus(tr("Playlist download failed."));
        emit playlistLoadFailed(token, result.error);
        return;
    }
    startParse(token, result.data, result.url);
}

void PlaylistManager::startParse(int playlistId, const QByteArray &data,
                                 const QString &origin)
{
    emit playlistStatus(tr("Parsing playlist..."));
    const int index = indexOfPlaylist(playlistId);
    if (index < 0)
        return;

    auto *watcher = new QFutureWatcher<M3UParser::Result>(this);
    m_pendingParses.append(watcher);
    connect(watcher, &QFutureWatcher<M3UParser::Result>::finished, this, [this, watcher,
                                                              playlistId, origin]() {
        const M3UParser::Result parse = watcher->result();
        m_pendingParses.removeAll(watcher);
        watcher->deleteLater();

        if (!parse.warnings.isEmpty())
            qInfo() << "[playlist] parse warnings for" << origin.left(120) << ":"
                    << parse.warnings.size() << "entries";

        if (parse.channels.isEmpty()) {
            qWarning().noquote()
                << "[playlist] no channels parsed from" << origin.left(120)
                << "skipped lines:" << parse.skippedLines;
            emit playlistStatus(tr("No channels found."));
            emit playlistLoadFailed(playlistId,
                                    tr("No channels were found in this playlist."));
            return;
        }

        QVector<Channel> channels = parse.channels;
        const QSet<QString> blocked = m_database->blockedUrls(playlistId);
        if (!blocked.isEmpty()) {
            channels.erase(
                std::remove_if(channels.begin(), channels.end(),
                               [&blocked](const Channel &c) {
                                   return blocked.contains(c.streamUrl());
                               }),
                channels.end());
        }

        const int count = channels.size();
        if (!m_database->replaceChannels(playlistId, channels)) {
            emit error(tr("Could not store playlist channels: %1")
                           .arg(m_database->lastError()));
            emit playlistLoadFailed(playlistId, tr("Could not save the playlist."));
            return;
        }

        const QDateTime now = QDateTime::currentDateTime();
        m_database->updateLastRefresh(playlistId, now);
        const int plIndex = indexOfPlaylist(playlistId);
        if (plIndex >= 0) {
            m_playlists[plIndex].channelCount = count;
            m_playlists[plIndex].lastRefresh = now;
        }
        m_channelCache.remove(playlistId);

        qInfo()
            << "[playlist] loaded" << count << "channels from" << origin.left(120);
        emit playlistsChanged();
        emit channelsChanged(playlistId);
        emit playlistLoaded(playlistId, count);
        emit playlistStatus(tr("Loaded %1 channels.").arg(count));
    });

    watcher->setFuture(QtConcurrent::run([data, origin]() {
        Q_UNUSED(origin)
        return M3UParser::parse(decodePayload(data));
    }));
}

QVector<Channel> PlaylistManager::favoriteChannels() const
{
    return m_database->favoriteChannels();
}

void PlaylistManager::addFavorite(const Channel &channel)
{
    if (!m_database->addFavorite(channel)) {
        emit error(tr("Could not save the favorite."));
        return;
    }
    m_favoriteUrls.insert(channel.url.toString());
    emit favoritesChanged();
}

void PlaylistManager::removeFavorite(const QString &url)
{
    if (!m_database->removeFavorite(url)) {
        emit error(tr("Could not remove the favorite."));
        return;
    }
    m_favoriteUrls.remove(url);
    emit favoritesChanged();
}

bool PlaylistManager::removeChannel(const Channel &channel)
{
    if (!m_database || channel.playlistId < 0 || !channel.isValid())
        return false;
    if (!m_database->blockChannel(channel))
        return false;
    if (!m_database->deleteChannel(channel.playlistId, channel.streamUrl()))
        return false;

    m_channelCache.remove(channel.playlistId);
    m_database->updateChannelCount(
        channel.playlistId,
        m_database->loadChannels(channel.playlistId).size());
    emit channelsChanged(channel.playlistId);
    emit playlistStatus(tr("Removed '%1' from the playlist.").arg(channel.name));
    return true;
}

bool PlaylistManager::restoreChannel(const Channel &channel)
{
    if (!m_database || channel.playlistId < 0 || !channel.isValid())
        return false;
    if (!m_database->unblockChannel(channel.playlistId, channel.streamUrl()))
        return false;
    if (!m_database->insertChannel(channel))
        return false;

    m_channelCache.remove(channel.playlistId);
    m_database->updateChannelCount(
        channel.playlistId,
        m_database->loadChannels(channel.playlistId).size());
    emit channelsChanged(channel.playlistId);
    emit playlistStatus(tr("Restored '%1'.").arg(channel.name));
    return true;
}

QVector<Channel> PlaylistManager::blockedChannels(int playlistId) const
{
    return m_database ? m_database->blockedChannels(playlistId)
                      : QVector<Channel>();
}

void PlaylistManager::setAutoRefreshMinutes(int minutes)
{
    m_autoRefreshTimer->stop();
    if (minutes <= 0)
        return;
    m_autoRefreshTimer->start(minutes * 60 * 1000);
    qInfo() << "[playlist] auto refresh interval set to" << minutes << "minutes";
}

void PlaylistManager::runAutoRefresh()
{
    const int interval = m_settings->autoRefreshMinutes();
    if (interval > 0) {
        const auto now = QDateTime::currentDateTime();
        for (const Playlist &p : std::as_const(m_playlists)) {
            if (!p.isRemote() || !p.enabled)
                continue;
            const bool stale = !p.lastRefresh.isValid() ||
                               p.lastRefresh.secsTo(now) >= interval * 60;
            if (stale)
                refreshPlaylist(p.id);
        }
    }
    if (interval > 0)
        m_autoRefreshTimer->start(interval * 60 * 1000);
}

void PlaylistManager::applyFavoriteFlags(QVector<Channel> *channels) const
{
    for (Channel &c : *channels)
        c.isFavorite = m_favoriteUrls.contains(c.url.toString());
}