#include "database/DatabaseManager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>
#include <QVariant>

#include "core/AppInfo.h"
#include "core/Constants.h"
#include "core/Logger.h"

namespace {

const char *kCreatePlaylists = R"SQL(
    CREATE TABLE IF NOT EXISTS playlists (
        id            INTEGER PRIMARY KEY AUTOINCREMENT,
        name          TEXT    NOT NULL,
        source_type   TEXT    NOT NULL,
        source        TEXT    NOT NULL,
        enabled       INTEGER NOT NULL DEFAULT 1,
        last_refresh  TEXT,
        channel_count INTEGER NOT NULL DEFAULT 0,
        created_at    TEXT    NOT NULL
    );
)SQL";

const char *kCreateChannels = R"SQL(
    CREATE TABLE IF NOT EXISTS channels (
        id          INTEGER PRIMARY KEY AUTOINCREMENT,
        playlist_id INTEGER NOT NULL,
        name        TEXT,
        group_title TEXT,
        url         TEXT,
        tvg_id      TEXT,
        tvg_name    TEXT,
        logo        TEXT,
        language    TEXT,
        country     TEXT,
        attrs_json  TEXT,
        UNIQUE (playlist_id, url)
    );
)SQL";

const char *kCreateIndex = R"SQL(
    CREATE INDEX IF NOT EXISTS idx_channels_playlist ON channels(playlist_id);
)SQL";

const char *kCreateFavorites = R"SQL(
    CREATE TABLE IF NOT EXISTS favorites (
        id          INTEGER PRIMARY KEY AUTOINCREMENT,
        channel_url TEXT    NOT NULL UNIQUE,
        name        TEXT    NOT NULL,
        group_title TEXT,
        logo        TEXT,
        tvg_id      TEXT,
        created_at  TEXT    NOT NULL
    );
)SQL";

const char *kCreateSchemaVersion = R"SQL(
    CREATE TABLE IF NOT EXISTS schema_version (
        version INTEGER NOT NULL
    );
)SQL";

const char *kCreateBlockedChannels = R"SQL(
    CREATE TABLE IF NOT EXISTS blocked_channels (
        id          INTEGER PRIMARY KEY AUTOINCREMENT,
        playlist_id INTEGER NOT NULL,
        name        TEXT,
        group_title TEXT,
        url         TEXT,
        tvg_id      TEXT,
        tvg_name    TEXT,
        logo        TEXT,
        language    TEXT,
        country     TEXT,
        attrs_json  TEXT,
        created_at  TEXT    NOT NULL,
        UNIQUE (playlist_id, url)
    );
)SQL";

constexpr int kCurrentSchemaVersion = 3;

QString jsonFromAttributes(const QHash<QString, QString> &attrs)
{
    if (attrs.isEmpty())
        return QString();
    QJsonObject obj;
    for (auto it = attrs.constBegin(); it != attrs.constEnd(); ++it)
        obj.insert(it.key(), it.value());
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

QHash<QString, QString> attributesFromJson(const QString &json)
{
    QHash<QString, QString> attrs;
    if (json.isEmpty())
        return attrs;
    const QJsonObject obj = QJsonDocument::fromJson(json.toUtf8()).object();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
        attrs.insert(it.key(), it.value().toString());
    return attrs;
}

} // namespace

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent),
      m_connectionName(QStringLiteral("copper-db-") +
                       QString::number(reinterpret_cast<quintptr>(this), 16))
{
}

DatabaseManager::~DatabaseManager()
{
    close();
}

bool DatabaseManager::open()
{
    if (isOpen())
        return true;

    const QString dataDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDir.isEmpty()) {
        reportError("open", QStringLiteral("No writable app data location."));
        return false;
    }

    const QString dbDir = dataDir + QLatin1Char('/') +
                          AppConstants::kDatabaseFileName;
    // dbDir is a file path; create its parent dir explicitly.
    QDir().mkpath(QFileInfo(dbDir).absolutePath());
    m_path = dbDir;

    m_db = new QSqlDatabase(QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                       m_connectionName));
    m_db->setDatabaseName(m_path);
    if (!m_db->open()) {
        reportError("open", m_db->lastError().text());
        delete m_db;
        m_db = nullptr;
        QSqlDatabase::removeDatabase(m_connectionName);
        return false;
    }

    {
        QSqlQuery pragma(*m_db);
        pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
        pragma.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
        pragma.exec(QStringLiteral("PRAGMA foreign_keys=ON"));
    }

    if (!createSchema() || !runMigrations()) {
        close();
        return false;
    }
    return true;
}

void DatabaseManager::close()
{
    if (!m_db)
        return;
    m_db->close();
    delete m_db;
    m_db = nullptr;
    QSqlDatabase::removeDatabase(m_connectionName);
    m_favoritesLoaded = false;
    m_favoriteCache.clear();
}

bool DatabaseManager::isOpen() const
{
    return m_db && m_db->isOpen();
}

QString DatabaseManager::databasePath() const
{
    return m_path;
}

QString DatabaseManager::lastError() const
{
    return m_lastError;
}

bool DatabaseManager::createSchema()
{
    if (!isOpen()) {
        reportError("schema", QStringLiteral("Database not open."));
        return false;
    }

    const char *statements[] = {kCreateSchemaVersion, kCreatePlaylists,
                                kCreateChannels, kCreateIndex, kCreateFavorites,
                                kCreateBlockedChannels};
    for (const char *sql : statements) {
        QSqlQuery q(*m_db);
        if (!q.exec(QString::fromLatin1(sql))) {
            reportError("schema", q.lastError().text());
            return false;
        }
    }
    return true;
}

bool DatabaseManager::runMigrations()
{
    if (!isOpen())
        return false;

    int version = 0;
    bool hasVersion = false;
    {
        // Scoped so the read statement is finalized (releasing its WAL read
        // transaction) before the schema-changing migration steps below: in
        // WAL mode a still-open read snapshot blocks DROP/CREATE with
        // SQLITE_LOCKED.
        QSqlQuery q(*m_db);
        if (!q.exec(QStringLiteral("SELECT version FROM schema_version LIMIT 1"))) {
            reportError("migration-select", q.lastError().text());
            return false;
        }
        if (q.next()) {
            version = q.value(0).toInt();
            hasVersion = true;
        }
    }

    if (!hasVersion) {
        // Fresh database: record the current schema version.
        QSqlQuery seed(*m_db);
        if (!seed.exec(QStringLiteral(
                "INSERT INTO schema_version (version) VALUES (%1)")
                .arg(kCurrentSchemaVersion))) {
            reportError("migration-seed", seed.lastError().text());
            return false;
        }
        return true;
    }

    // Migrations below run in ascending order.
    if (version < 2) {
        // v1 -> v2: channels with a missing name/group_title must still be
        // stored. Relax the NOT NULL constraints (the parser leaves these
        // null when the m3u attributes are absent).
        if (!m_db->transaction()) {
            reportError("migration-txn", m_db->lastError().text());
            return false;
        }
        const QStringList steps = {
            QStringLiteral(
                "CREATE TABLE channels_v2 ("
                "id          INTEGER PRIMARY KEY AUTOINCREMENT, "
                "playlist_id INTEGER NOT NULL, "
                "name        TEXT, "
                "group_title TEXT, "
                "url         TEXT, "
                "tvg_id      TEXT, "
                "tvg_name    TEXT, "
                "logo        TEXT, "
                "language    TEXT, "
                "country     TEXT, "
                "attrs_json  TEXT, "
                "UNIQUE (playlist_id, url))"),
            QStringLiteral(
                "INSERT INTO channels_v2 (id, playlist_id, name, group_title, "
                "url, tvg_id, tvg_name, logo, language, country, attrs_json) "
                "SELECT id, playlist_id, name, group_title, url, tvg_id, "
                "tvg_name, logo, language, country, attrs_json FROM channels"),
            QStringLiteral("DROP TABLE channels"),
            QStringLiteral("ALTER TABLE channels_v2 RENAME TO channels"),
            QStringLiteral(
                "CREATE INDEX IF NOT EXISTS idx_channels_playlist "
                "ON channels(playlist_id)"),
            QStringLiteral("UPDATE schema_version SET version = 2")
        };
        for (int i = 0; i < steps.size(); ++i) {
            QSqlQuery stepQuery(*m_db);
            if (!stepQuery.exec(steps.at(i))) {
                m_db->rollback();
                const QString where =
                    QStringLiteral("migration-step-%1").arg(i) +
                    QLatin1Char('(') +
                    stepQuery.lastError().nativeErrorCode() +
                    QLatin1Char(')');
                reportError(where, stepQuery.lastError().text());
                return false;
            }
        }
        m_db->commit();
    }
    if (version < 3) {
        // v2 -> v3: track removed (blocked) channels so a refresh does not
        // resurrect them. Stored snapshot of the channel for the restore UI.
        if (!m_db->transaction()) {
            reportError("migration-txn", m_db->lastError().text());
            return false;
        }
        const QStringList steps = {
            QString(kCreateBlockedChannels),
            QStringLiteral(
                "UPDATE schema_version SET version = 3")
        };
        for (int i = 0; i < steps.size(); ++i) {
            QSqlQuery stepQuery(*m_db);
            if (!stepQuery.exec(steps.at(i))) {
                m_db->rollback();
                const QString where =
                    QStringLiteral("migration-step-%1").arg(i) +
                    QLatin1Char('(') +
                    stepQuery.lastError().nativeErrorCode() +
                    QLatin1Char(')');
                reportError(where, stepQuery.lastError().text());
                return false;
            }
        }
        m_db->commit();
    }
    return true;
}

void DatabaseManager::reportError(const QString &where, const QString &message)
{
    m_lastError = QStringLiteral("%1: %2").arg(where, message);
    qWarning().noquote() << "[database]" << m_lastError;
    emit error(m_lastError);
}

QVector<Playlist> DatabaseManager::loadPlaylists()
{
    QVector<Playlist> out;
    if (!isOpen())
        return out;

    QSqlQuery q(*m_db);
    if (!q.exec(QStringLiteral(
            "SELECT id, name, source_type, source, enabled, last_refresh, "
            "channel_count FROM playlists ORDER BY id"))) {
        reportError("loadPlaylists", q.lastError().text());
        return out;
    }

    while (q.next()) {
        Playlist p;
        p.id = q.value(0).toInt();
        p.name = q.value(1).toString();
        p.type = q.value(2).toString() == QLatin1String("remote")
                     ? Playlist::SourceType::Remote
                     : Playlist::SourceType::Local;
        p.source = q.value(3).toString();
        p.enabled = q.value(4).toBool();
        const QString refresh = q.value(5).toString();
        if (!refresh.isEmpty())
            p.lastRefresh = QDateTime::fromString(refresh, Qt::ISODate);
        p.channelCount = q.value(6).toInt();
        out.append(p);
    }
    return out;
}

int DatabaseManager::addPlaylist(const Playlist &playlist)
{
    if (!isOpen())
        return -1;

    QSqlQuery q(*m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO playlists (name, source_type, source, enabled, "
        "last_refresh, channel_count, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(playlist.name);
    q.addBindValue(playlist.isRemote() ? QStringLiteral("remote")
                                       : QStringLiteral("local"));
    q.addBindValue(playlist.source);
    q.addBindValue(playlist.enabled ? 1 : 0);
    q.addBindValue(playlist.lastRefresh.isValid()
                       ? playlist.lastRefresh.toString(Qt::ISODate)
                       : QVariant());
    q.addBindValue(playlist.channelCount);
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));

    if (!q.exec()) {
        reportError("addPlaylist", q.lastError().text());
        return -1;
    }
    return q.lastInsertId().toInt();
}

bool DatabaseManager::updatePlaylist(const Playlist &playlist)
{
    if (!isOpen() || playlist.id < 0)
        return false;

    QSqlQuery q(*m_db);
    q.prepare(QStringLiteral(
        "UPDATE playlists SET name = ?, source_type = ?, source = ?, "
        "enabled = ? WHERE id = ?"));
    q.addBindValue(playlist.name);
    q.addBindValue(playlist.isRemote() ? QStringLiteral("remote")
                                       : QStringLiteral("local"));
    q.addBindValue(playlist.source);
    q.addBindValue(playlist.enabled ? 1 : 0);
    q.addBindValue(playlist.id);
    if (!q.exec()) {
        reportError("updatePlaylist", q.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseManager::removePlaylist(int playlistId)
{
    if (!isOpen())
        return false;

    QSqlQuery q(*m_db);
    if (!q.exec(QStringLiteral("DELETE FROM channels WHERE playlist_id = %1")
                    .arg(playlistId))) {
        reportError("removePlaylist", q.lastError().text());
        return false;
    }
    if (!q.exec(QStringLiteral(
                    "DELETE FROM blocked_channels WHERE playlist_id = %1")
                    .arg(playlistId))) {
        reportError("removePlaylist", q.lastError().text());
        return false;
    }
    if (!q.exec(QStringLiteral("DELETE FROM playlists WHERE id = %1")
                    .arg(playlistId))) {
        reportError("removePlaylist", q.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseManager::updateChannelCount(int playlistId, int count)
{
    if (!isOpen())
        return false;
    QSqlQuery q(*m_db);
    q.prepare(QStringLiteral("UPDATE playlists SET channel_count = ? WHERE id = ?"));
    q.addBindValue(count);
    q.addBindValue(playlistId);
    if (!q.exec()) {
        reportError("updateChannelCount", q.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseManager::updateLastRefresh(int playlistId, const QDateTime &when)
{
    if (!isOpen())
        return false;
    QSqlQuery q(*m_db);
    q.prepare(QStringLiteral("UPDATE playlists SET last_refresh = ? WHERE id = ?"));
    q.addBindValue(when.toString(Qt::ISODate));
    q.addBindValue(playlistId);
    if (!q.exec()) {
        reportError("updateLastRefresh", q.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseManager::replaceChannels(int playlistId,
                                      const QVector<Channel> &channels)
{
    if (!isOpen())
        return false;

    if (!m_db->transaction()) {
        reportError("replaceChannels", m_db->lastError().text());
        return false;
    }

    QSqlQuery del(*m_db);
    del.prepare(QStringLiteral("DELETE FROM channels WHERE playlist_id = ?"));
    del.addBindValue(playlistId);
    if (!del.exec()) {
        reportError("replaceChannels", del.lastError().text());
        m_db->rollback();
        return false;
    }

    QSqlQuery ins(*m_db);
    ins.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO channels (playlist_id, name, group_title, url, "
        "tvg_id, tvg_name, logo, language, country, attrs_json) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    for (const Channel &c : channels) {
        // SQLite treats null QStrings as NULL, which violates the NOT NULL
        // constraints on name/group_title/url; store empty strings instead.
        QString name = c.name;
        if (name.isNull())
            name.clear();
        QString groupTitle = c.groupTitle;
        if (groupTitle.isNull())
            groupTitle.clear();
        QString url = c.url.toString();
        if (url.isNull())
            url.clear();
        ins.bindValue(0, playlistId);
        ins.bindValue(1, name);
        ins.bindValue(2, groupTitle);
        ins.bindValue(3, url);
        ins.bindValue(4, c.tvgId);
        ins.bindValue(5, c.tvgName);
        ins.bindValue(6, c.logoUrl);
        ins.bindValue(7, c.language);
        ins.bindValue(8, c.country);
        ins.bindValue(9, jsonFromAttributes(c.attributes));
        if (!ins.exec()) {
            reportError("replaceChannels", ins.lastError().text());
            m_db->rollback();
            return false;
        }
    }

    if (!m_db->commit()) {
        reportError("replaceChannels", m_db->lastError().text());
        m_db->rollback();
        return false;
    }

    updateChannelCount(playlistId, channels.size());
    return true;
}

QVector<Channel> DatabaseManager::loadChannels(int playlistId)
{
    QVector<Channel> out;
    if (!isOpen())
        return out;

    QSqlQuery q(*m_db);
    q.prepare(QStringLiteral(
        "SELECT name, group_title, url, tvg_id, tvg_name, logo, language, "
        "country, attrs_json FROM channels WHERE playlist_id = ? ORDER BY id"));
    q.addBindValue(playlistId);
    if (!q.exec()) {
        reportError("loadChannels", q.lastError().text());
        return out;
    }

    while (q.next()) {
        Channel c;
        c.name = q.value(0).toString();
        c.groupTitle = q.value(1).toString();
        c.url = QUrl(q.value(2).toString());
        c.tvgId = q.value(3).toString();
        c.tvgName = q.value(4).toString();
        c.logoUrl = q.value(5).toString();
        c.language = q.value(6).toString();
        c.country = q.value(7).toString();
        c.attributes = attributesFromJson(q.value(8).toString());
        c.playlistId = playlistId;
        out.append(c);
    }
    return out;
}

bool DatabaseManager::insertChannel(const Channel &channel)
{
    if (!isOpen() || channel.playlistId < 0 || !channel.isValid())
        return false;

    QSqlQuery q(*m_db);
    q.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO channels (playlist_id, name, group_title, url, "
        "tvg_id, tvg_name, logo, language, country, attrs_json) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    QString name = channel.name;
    if (name.isNull())
        name.clear();
    QString groupTitle = channel.groupTitle;
    if (groupTitle.isNull())
        groupTitle.clear();
    QString url = channel.url.toString();
    if (url.isNull())
        url.clear();
    q.addBindValue(channel.playlistId);
    q.addBindValue(name);
    q.addBindValue(groupTitle);
    q.addBindValue(url);
    q.addBindValue(channel.tvgId);
    q.addBindValue(channel.tvgName);
    q.addBindValue(channel.logoUrl);
    q.addBindValue(channel.language);
    q.addBindValue(channel.country);
    q.addBindValue(jsonFromAttributes(channel.attributes));
    if (!q.exec()) {
        reportError("insertChannel", q.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseManager::deleteChannel(int playlistId, const QString &url)
{
    if (!isOpen())
        return false;
    QSqlQuery q(*m_db);
    q.prepare(QStringLiteral(
        "DELETE FROM channels WHERE playlist_id = ? AND url = ?"));
    q.addBindValue(playlistId);
    q.addBindValue(url);
    if (!q.exec()) {
        reportError("deleteChannel", q.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseManager::blockChannel(const Channel &channel)
{
    if (!isOpen() || channel.playlistId < 0 || !channel.isValid())
        return false;

    QSqlQuery q(*m_db);
    q.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO blocked_channels (playlist_id, name, "
        "group_title, url, tvg_id, tvg_name, logo, language, country, "
        "attrs_json, created_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    QString name = channel.name;
    if (name.isNull())
        name.clear();
    QString groupTitle = channel.groupTitle;
    if (groupTitle.isNull())
        groupTitle.clear();
    QString url = channel.url.toString();
    if (url.isNull())
        url.clear();
    q.addBindValue(channel.playlistId);
    q.addBindValue(name);
    q.addBindValue(groupTitle);
    q.addBindValue(url);
    q.addBindValue(channel.tvgId);
    q.addBindValue(channel.tvgName);
    q.addBindValue(channel.logoUrl);
    q.addBindValue(channel.language);
    q.addBindValue(channel.country);
    q.addBindValue(jsonFromAttributes(channel.attributes));
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    if (!q.exec()) {
        reportError("blockChannel", q.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseManager::unblockChannel(int playlistId, const QString &url)
{
    if (!isOpen())
        return false;
    QSqlQuery q(*m_db);
    q.prepare(QStringLiteral(
        "DELETE FROM blocked_channels WHERE playlist_id = ? AND url = ?"));
    q.addBindValue(playlistId);
    q.addBindValue(url);
    if (!q.exec()) {
        reportError("unblockChannel", q.lastError().text());
        return false;
    }
    return true;
}

QVector<Channel> DatabaseManager::blockedChannels(int playlistId)
{
    QVector<Channel> out;
    if (!isOpen())
        return out;

    QSqlQuery q(*m_db);
    q.prepare(QStringLiteral(
        "SELECT name, group_title, url, tvg_id, tvg_name, logo, language, "
        "country, attrs_json FROM blocked_channels WHERE playlist_id = ? "
        "ORDER BY id"));
    q.addBindValue(playlistId);
    if (!q.exec()) {
        reportError("blockedChannels", q.lastError().text());
        return out;
    }

    while (q.next()) {
        Channel c;
        c.name = q.value(0).toString();
        c.groupTitle = q.value(1).toString();
        c.url = QUrl(q.value(2).toString());
        c.tvgId = q.value(3).toString();
        c.tvgName = q.value(4).toString();
        c.logoUrl = q.value(5).toString();
        c.language = q.value(6).toString();
        c.country = q.value(7).toString();
        c.attributes = attributesFromJson(q.value(8).toString());
        c.playlistId = playlistId;
        out.append(c);
    }
    return out;
}

QSet<QString> DatabaseManager::blockedUrls(int playlistId)
{
    QSet<QString> out;
    if (!isOpen())
        return out;

    QSqlQuery q(*m_db);
    q.prepare(QStringLiteral(
        "SELECT url FROM blocked_channels WHERE playlist_id = ?"));
    q.addBindValue(playlistId);
    if (!q.exec()) {
        reportError("blockedUrls", q.lastError().text());
        return out;
    }
    while (q.next())
        out.insert(q.value(0).toString());
    return out;
}

QSet<QString> DatabaseManager::favoriteUrls()
{
    if (!isOpen())
        return {};
    if (m_favoritesLoaded)
        return m_favoriteCache;

    m_favoriteCache.clear();
    QSqlQuery q(*m_db);
    if (!q.exec(QStringLiteral("SELECT channel_url FROM favorites"))) {
        reportError("favoriteUrls", q.lastError().text());
        return m_favoriteCache;
    }
    while (q.next())
        m_favoriteCache.insert(q.value(0).toString());
    m_favoritesLoaded = true;
    return m_favoriteCache;
}

QVector<Channel> DatabaseManager::favoriteChannels()
{
    QVector<Channel> out;
    if (!isOpen())
        return out;

    QSqlQuery q(*m_db);
    if (!q.exec(QStringLiteral(
            "SELECT channel_url, name, group_title, logo, tvg_id FROM favorites "
            "ORDER BY created_at"))) {
        reportError("favoriteChannels", q.lastError().text());
        return out;
    }

    while (q.next()) {
        Channel c;
        c.url = QUrl(q.value(0).toString());
        c.name = q.value(1).toString();
        c.groupTitle = q.value(2).toString();
        c.logoUrl = q.value(3).toString();
        c.tvgId = q.value(4).toString();
        out.append(c);
    }
    return out;
}

bool DatabaseManager::isFavorite(const QString &url) const
{
    if (!m_favoritesLoaded)
        const_cast<DatabaseManager *>(this)->favoriteUrls();
    return m_favoriteCache.contains(url);
}

bool DatabaseManager::addFavorite(const Channel &channel)
{
    if (!isOpen())
        return false;

    QSqlQuery q(*m_db);
    q.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO favorites (channel_url, name, group_title, "
        "logo, tvg_id, created_at) VALUES (?, ?, ?, ?, ?, ?)"));
    q.addBindValue(channel.url.toString());
    q.addBindValue(channel.name);
    q.addBindValue(channel.groupTitle);
    q.addBindValue(channel.logoUrl);
    q.addBindValue(channel.tvgId);
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    if (!q.exec()) {
        reportError("addFavorite", q.lastError().text());
        return false;
    }
    m_favoriteCache.insert(channel.url.toString());
    m_favoritesLoaded = true;
    return true;
}

bool DatabaseManager::removeFavorite(const QString &url)
{
    if (!isOpen())
        return false;

    QSqlQuery q(*m_db);
    q.prepare(QStringLiteral("DELETE FROM favorites WHERE channel_url = ?"));
    q.addBindValue(url);
    if (!q.exec()) {
        reportError("removeFavorite", q.lastError().text());
        return false;
    }
    m_favoriteCache.remove(url);
    m_favoritesLoaded = true;
    return true;
}