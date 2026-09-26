#include <QtTest>

#include "models/Channel.h"
#include "models/ChannelGroup.h"

// Tests for the channel/group helper functions used by the playlist layer.
// Pure value logic: no widgets, no database, no network.
class TestChannelUtils : public QObject
{
    Q_OBJECT
private slots:
    void groupForUsesFallback();
    void buildGroupsOrdersAndCounts();
    void matchesSearchFields();
    void matchesSearchIsCaseInsensitive();
    void emptyQueryMatchesEverything();
    void maskedUrlRemovesCredentials();
    void isStreamUrlDetection();
};

Channel makeChannel(const QString &name, const QString &group,
                    const QString &tvgId = QString(),
                    const QString &language = QString(),
                    const QString &country = QString())
{
    Channel channel;
    channel.name = name;
    channel.groupTitle = group;
    channel.tvgId = tvgId;
    channel.language = language;
    channel.country = country;
    channel.url = QUrl(QStringLiteral("http://example.com/%1.ts").arg(name));
    return channel;
}

void TestChannelUtils::groupForUsesFallback()
{
    Channel withGroup = makeChannel(QStringLiteral("N"), QStringLiteral("News"));
    QCOMPARE(ChannelUtils::groupFor(withGroup), QStringLiteral("News"));

    Channel none = makeChannel(QStringLiteral("X"), QString());
    QCOMPARE(ChannelUtils::groupFor(none), QStringLiteral("Uncategorized"));
}

void TestChannelUtils::buildGroupsOrdersAndCounts()
{
    QVector<Channel> channels;
    channels.append(makeChannel(QStringLiteral("A"), QStringLiteral("News")));
    channels.append(makeChannel(QStringLiteral("B"), QStringLiteral("Sports")));
    channels.append(makeChannel(QStringLiteral("C"), QStringLiteral("News")));
    channels.append(makeChannel(QStringLiteral("D"), QString()));

    const QVector<ChannelGroup> groups = ChannelUtils::buildGroups(channels);

    QCOMPARE(groups.size(), 3);
    QCOMPARE(groups.at(0).name(), QStringLiteral("News"));
    QCOMPARE(groups.at(0).channelCount(), 2);
    QCOMPARE(groups.at(1).name(), QStringLiteral("Sports"));
    QCOMPARE(groups.at(1).channelCount(), 1);
    QCOMPARE(groups.at(2).name(), QStringLiteral("Uncategorized"));
    QCOMPARE(groups.at(2).channelCount(), 1);
}

void TestChannelUtils::matchesSearchFields()
{
    const Channel channel = makeChannel(
        QStringLiteral("BBC World"),
        QStringLiteral("News"),
        QStringLiteral("bbcworld"),
        QStringLiteral("English"),
        QStringLiteral("UK"));

    QVERIFY(ChannelUtils::matchesSearch(channel, QStringLiteral("bbc")));
    QVERIFY(ChannelUtils::matchesSearch(channel, QStringLiteral("news")));
    QVERIFY(ChannelUtils::matchesSearch(channel, QStringLiteral("english")));
    QVERIFY(ChannelUtils::matchesSearch(channel, QStringLiteral("uk")));
    QVERIFY(!ChannelUtils::matchesSearch(channel, QStringLiteral("sports")));
}

void TestChannelUtils::matchesSearchIsCaseInsensitive()
{
    const Channel channel = makeChannel(QStringLiteral("CNN HD"), QStringLiteral("News"));
    QVERIFY(ChannelUtils::matchesSearch(channel, QStringLiteral("cnn")));
    QVERIFY(ChannelUtils::matchesSearch(channel, QStringLiteral("CNN")));
    QVERIFY(ChannelUtils::matchesSearch(channel, QStringLiteral("Nn Hd")));
}

void TestChannelUtils::emptyQueryMatchesEverything()
{
    QVERIFY(ChannelUtils::matchesSearch(
        makeChannel(QStringLiteral("Anything"), QString()), QString()));
}

void TestChannelUtils::maskedUrlRemovesCredentials()
{
    QCOMPARE(ChannelUtils::maskedUrl(
                 QStringLiteral("http://user:pass@example.com/a.ts")),
             QStringLiteral("http://example.com/a.ts"));
    QCOMPARE(ChannelUtils::maskedUrl(
                 QStringLiteral("https://example.com/b.ts")),
             QStringLiteral("https://example.com/b.ts"));
}

void TestChannelUtils::isStreamUrlDetection()
{
    QVERIFY(ChannelUtils::isStreamUrl(QStringLiteral("http://example.com/a.ts")));
    QVERIFY(ChannelUtils::isStreamUrl(QStringLiteral("HTTPS://example.com/b.ts")));
    QVERIFY(!ChannelUtils::isStreamUrl(QStringLiteral("rtmp://example.com/c")));
    QVERIFY(!ChannelUtils::isStreamUrl(QStringLiteral("C:\\videos\\movie.mp4")));
}

QTEST_GUILESS_MAIN(TestChannelUtils)
#include "test_channelutils.moc"