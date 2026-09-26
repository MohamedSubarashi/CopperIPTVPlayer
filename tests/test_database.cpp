#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QtTest>
#include <QVariant>

#include "database/DatabaseManager.h"

// Regression tests for cached channel persistence. These mirror the failure
// seen in production where real playlists contain channels without a
// group-title (or name) attribute, or duplicate stream URLs, and the save
// previously aborted / silently dropped rows because those columns were
// NOT NULL and the SQLite driver bound empty strings as NULL.
class TestDatabaseManager : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

    void storesChannelsWithMissingGroups();
    void ignoresDuplicateUrls();
    void storesChannelWithOnlyUrl();
    void migratesLegacyNotNullSchema();
    void blocksAndUnblocksChannels();
    void restoresBlockedChannel();
    void migrationV3CreatesBlockedTable();
};

void TestDatabaseManager::initTestCase()
{
    // Route the test database into a dedicated location so the real app data
    // directory is never touched.
    QCoreApplication::setOrganizationName(QStringLiteral("CopperTests"));
    QCoreApplication::setApplicationName(
        QStringLiteral("CopperIPTVPlayerTests"));
}

void TestDatabaseManager::cleanupTestCase()
{
    const QString dbDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString db = QDir(dbDir).filePath(QStringLiteral("copper.db"));
    QFile::remove(db);
    QFile::remove(db + QLatin1String("-wal"));
    QFile::remove(db + QLatin1String("-shm"));
}

void TestDatabaseManager::init()
{
    cleanupTestCase();
}

void TestDatabaseManager::storesChannelsWithMissingGroups()
{
    DatabaseManager db;
    QVERIFY(db.open());

    Playlist playlist;
    playlist.name = QStringLiteral("test");
    playlist.source = QStringLiteral("test.m3u");
    const int id = db.addPlaylist(playlist);
    QVERIFY(id >= 0);

    Channel withGroup;
    withGroup.name = QStringLiteral("Grouped");
    withGroup.groupTitle = QStringLiteral("News");
    withGroup.url = QUrl(QStringLiteral("http://example.com/grouped.ts"));

    Channel noGroup;
    noGroup.name = QStringLiteral("Ungrouped");
    noGroup.url = QUrl(QStringLiteral("http://example.com/ungrouped.ts"));

    Channel noName;
    noName.groupTitle = QStringLiteral("Sports");
    noName.url = QUrl(QStringLiteral("http://example.com/noname.ts"));

    QVector<Channel> channels = {withGroup, noGroup, noName};
    QVERIFY(db.replaceChannels(id, channels));

    const QVector<Channel> loaded = db.loadChannels(id);
    QCOMPARE(loaded.size(), 3);
    QCOMPARE(loaded.at(0).name, QStringLiteral("Grouped"));
    QCOMPARE(loaded.at(0).groupTitle, QStringLiteral("News"));
    QCOMPARE(loaded.at(1).name, QStringLiteral("Ungrouped"));
    QCOMPARE(loaded.at(1).groupTitle, QString());
    QCOMPARE(loaded.at(2).name, QString());
    QCOMPARE(loaded.at(2).groupTitle, QStringLiteral("Sports"));
}

void TestDatabaseManager::ignoresDuplicateUrls()
{
    DatabaseManager db;
    QVERIFY(db.open());

    Playlist playlist;
    playlist.name = QStringLiteral("dup");
    playlist.source = QStringLiteral("dup.m3u");
    const int id = db.addPlaylist(playlist);
    QVERIFY(id >= 0);

    Channel a;
    a.name = QStringLiteral("A");
    a.groupTitle = QStringLiteral("News");
    a.url = QUrl(QStringLiteral("http://example.com/same.ts"));

    Channel b;
    b.name = QStringLiteral("B");
    b.groupTitle = QStringLiteral("News");
    b.url = QUrl(QStringLiteral("http://example.com/same.ts"));

    QVERIFY(db.replaceChannels(id, {a, b}));

    const QVector<Channel> loaded = db.loadChannels(id);
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(loaded.first().name, QStringLiteral("A"));
}

void TestDatabaseManager::storesChannelWithOnlyUrl()
{
    DatabaseManager db;
    QVERIFY(db.open());

    Playlist playlist;
    playlist.name = QStringLiteral("bare");
    playlist.source = QStringLiteral("bare.m3u");
    const int id = db.addPlaylist(playlist);
    QVERIFY(id >= 0);

    Channel bare;
    bare.url = QUrl(QStringLiteral("http://example.com/bare.ts"));

    QVERIFY(db.replaceChannels(id, {bare}));

    const QVector<Channel> loaded = db.loadChannels(id);
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(loaded.first().name, QString());
    QCOMPARE(loaded.first().groupTitle, QString());
    QCOMPARE(loaded.first().url, QUrl(QStringLiteral("http://example.com/bare.ts")));
}

void TestDatabaseManager::migratesLegacyNotNullSchema()
{
    const QString dbPath = QStandardPaths::writableLocation(
                               QStandardPaths::AppDataLocation) +
                           QStringLiteral("/copper.db");
    QFile::remove(dbPath);
    QFile::remove(dbPath + QLatin1String("-wal"));
    QFile::remove(dbPath + QLatin1String("-shm"));

    {
        // Build a database matching the pre-fix v1 schema (NOT NULL columns).
        {
            QSqlDatabase raw = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"), QStringLiteral("legacy"));
            raw.setDatabaseName(dbPath);
            QVERIFY(raw.open());
            {
                QSqlQuery q(raw);
                QVERIFY(q.exec(QStringLiteral(
                    "CREATE TABLE channels ("
                    "id          INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "playlist_id INTEGER NOT NULL, "
                    "name        TEXT    NOT NULL, "
                    "group_title TEXT    NOT NULL, "
                    "url         TEXT    NOT NULL, "
                    "tvg_id      TEXT, tvg_name TEXT, logo TEXT, language "
                    "TEXT, country TEXT, attrs_json TEXT, UNIQUE (playlist_id, "
                    "url))")));
                QVERIFY(q.exec(QStringLiteral(
                    "CREATE TABLE schema_version (version INTEGER NOT NULL)")));
                QVERIFY(q.exec(QStringLiteral(
                    "INSERT INTO schema_version (version) VALUES (1)")));
                QVERIFY(q.exec(QStringLiteral(
                    "INSERT INTO channels (playlist_id, name, group_title, "
                    "url) VALUES (1, 'kept', 'News', "
                    "'http://example.com/kept.ts')")));
            }
            raw.close();
        }
        // The connection handle is gone; only now unregister it.
        QSqlDatabase::removeDatabase(QStringLiteral("legacy"));
    }

    {
        DatabaseManager db;
        QVERIFY(db.open());

        int version = 0;
        bool nameNullable = false;
        bool groupNullable = false;
        {
            QSqlDatabase raw = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"), QStringLiteral("legacy2"));
            raw.setDatabaseName(dbPath);
            QVERIFY(raw.open());
            {
                QSqlQuery q(raw);
                QVERIFY(q.exec(QStringLiteral(
                    "SELECT version FROM schema_version LIMIT 1")));
                QVERIFY(q.next());
                version = q.value(0).toInt();
                QVERIFY(q.exec(QStringLiteral("PRAGMA table_info(channels)")));
                while (q.next()) {
                    if (q.value(1).toString() == QLatin1String("name"))
                        nameNullable = q.value(3).toInt() == 0;
                    if (q.value(1).toString() == QLatin1String("group_title"))
                        groupNullable = q.value(3).toInt() == 0;
                }
            }
            raw.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("legacy2"));

        QCOMPARE(version, 3);
        QVERIFY(nameNullable);
        QVERIFY(groupNullable);

        // Pre-existing rows survive the rebuild.
        QCOMPARE(db.loadChannels(1).size(), 1);

        // New rows with missing name/group now persist.
        const QVector<Channel> loaded = db.loadChannels(1);
        QCOMPARE(loaded.first().name, QStringLiteral("kept"));
        QVERIFY(db.replaceChannels(1, {loaded.first()}));
        QCOMPARE(db.loadChannels(1).size(), 1);
    }
}

void TestDatabaseManager::blocksAndUnblocksChannels()
{
    DatabaseManager db;
    QVERIFY(db.open());

    Playlist playlist;
    playlist.name = QStringLiteral("block");
    playlist.source = QStringLiteral("block.m3u");
    const int id = db.addPlaylist(playlist);
    QVERIFY(id >= 0);

    Channel victim;
    victim.name = QStringLiteral("Victim");
    victim.groupTitle = QStringLiteral("News");
    victim.url = QUrl(QStringLiteral("http://example.com/victim.ts"));
    victim.playlistId = id;
    QVERIFY(db.insertChannel(victim));

    QVERIFY(db.blockChannel(victim));
    QVERIFY(db.blockedUrls(id).contains(victim.streamUrl()));
    QCOMPARE(db.blockedChannels(id).size(), 1);
    QCOMPARE(db.blockedChannels(id).first().name, QStringLiteral("Victim"));
    // Re-blocking the same channel is idempotent (INSERT OR IGNORE).
    QVERIFY(db.blockChannel(victim));
    QCOMPARE(db.blockedChannels(id).size(), 1);

    QVERIFY(db.unblockChannel(id, victim.streamUrl()));
    QVERIFY(db.blockedUrls(id).isEmpty());
    QVERIFY(db.blockedChannels(id).isEmpty());
}

void TestDatabaseManager::restoresBlockedChannel()
{
    DatabaseManager db;
    QVERIFY(db.open());

    Playlist playlist;
    playlist.name = QStringLiteral("restore");
    playlist.source = QStringLiteral("restore.m3u");
    const int id = db.addPlaylist(playlist);
    QVERIFY(id >= 0);

    Channel channel;
    channel.name = QStringLiteral("News One");
    channel.groupTitle = QStringLiteral("News");
    channel.logoUrl = QStringLiteral("http://example.com/logo.png");
    channel.tvgId = QStringLiteral("news1");
    channel.url = QUrl(QStringLiteral("http://example.com/news1.ts"));
    channel.playlistId = id;
    QVERIFY(db.insertChannel(channel));
    QCOMPARE(db.loadChannels(id).size(), 1);

    // Blocking removes the channel from the snapshot and records it.
    QVERIFY(db.blockChannel(channel));
    QVERIFY(db.deleteChannel(id, channel.streamUrl()));
    QCOMPARE(db.loadChannels(id).size(), 0);
    QCOMPARE(db.blockedChannels(id).size(), 1);

    // Restore = unblock + re-insert; the snapshot row comes back.
    QVERIFY(db.unblockChannel(id, channel.streamUrl()));
    QVERIFY(db.insertChannel(channel));
    const QVector<Channel> loaded = db.loadChannels(id);
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(loaded.first().name, QStringLiteral("News One"));
    QCOMPARE(loaded.first().groupTitle, QStringLiteral("News"));
    QCOMPARE(loaded.first().logoUrl,
             QStringLiteral("http://example.com/logo.png"));
    QCOMPARE(loaded.first().tvgId, QStringLiteral("news1"));
}

void TestDatabaseManager::migrationV3CreatesBlockedTable()
{
    const QString dbPath = QStandardPaths::writableLocation(
                               QStandardPaths::AppDataLocation) +
                           QStringLiteral("/copper.db");
    QFile::remove(dbPath);
    QFile::remove(dbPath + QLatin1String("-wal"));
    QFile::remove(dbPath + QLatin1String("-shm"));

    {
        {
            // A database at the current v2 schema (nullable channels).
            QSqlDatabase raw = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"), QStringLiteral("legacyV2"));
            raw.setDatabaseName(dbPath);
            QVERIFY(raw.open());
            {
                QSqlQuery q(raw);
                QVERIFY(q.exec(QStringLiteral(
                    "CREATE TABLE channels ("
                    "id          INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "playlist_id INTEGER NOT NULL, "
                    "name        TEXT, "
                    "group_title TEXT, "
                    "url         TEXT, "
                    "tvg_id      TEXT, tvg_name TEXT, logo TEXT, language "
                    "TEXT, country TEXT, attrs_json TEXT, UNIQUE (playlist_id, "
                    "url))")));
                QVERIFY(q.exec(QStringLiteral(
                    "CREATE TABLE schema_version (version INTEGER NOT NULL)")));
                QVERIFY(q.exec(QStringLiteral(
                    "INSERT INTO schema_version (version) VALUES (2)")));
            }
            raw.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("legacyV2"));
    }

    {
        DatabaseManager db;
        QVERIFY(db.open());

        int version = 0;
        bool blockedTable = false;
        {
            QSqlDatabase raw = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"), QStringLiteral("legacyV2b"));
            raw.setDatabaseName(dbPath);
            QVERIFY(raw.open());
            {
                QSqlQuery q(raw);
                QVERIFY(q.exec(QStringLiteral(
                    "SELECT version FROM schema_version LIMIT 1")));
                QVERIFY(q.next());
                version = q.value(0).toInt();
                QVERIFY(q.exec(QStringLiteral(
                    "SELECT name FROM sqlite_master WHERE type='table' AND "
                    "name='blocked_channels'")));
                blockedTable = q.next();
            }
            raw.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("legacyV2b"));

        QCOMPARE(version, 3);
        QVERIFY(blockedTable);

        // The new table is usable immediately after the migration.
        Playlist playlist;
        playlist.name = QStringLiteral("mig");
        playlist.source = QStringLiteral("mig.m3u");
        const int id = db.addPlaylist(playlist);
        QVERIFY(id >= 0);
        Channel channel;
        channel.name = QStringLiteral("Migrated");
        channel.url = QUrl(QStringLiteral("http://example.com/mig.ts"));
        channel.playlistId = id;
        QVERIFY(db.insertChannel(channel));
        QVERIFY(db.blockChannel(channel));
        QCOMPARE(db.blockedChannels(id).size(), 1);
    }
}

QTEST_GUILESS_MAIN(TestDatabaseManager)
#include "test_database.moc"