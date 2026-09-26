#include <QtTest>

#include "playlist/M3UParser.h"

// Unit tests for the tolerant M3U/M3U8 parser. Everything here is pure string
// processing, so there is no need for a Qt event loop or network.
class TestM3UParser : public QObject
{
    Q_OBJECT
private slots:
    void parsesExtendedM3U();
    void parsesPlainM3U();
    void readsAttributes();
    void extgrOverridesGroup();
    void nameFallsBackToAttributes();
    void handlesCrLfAndWhitespace();
    void skipsMalformedContent();
    void nameCanContainQuotedCommas();
    void stripsBom();
};

void TestM3UParser::parsesExtendedM3U()
{
    const QString content = QStringLiteral(
        "#EXTM3U\n"
        "#EXTINF:-1 tvg-id=\"1\" group-title=\"News\",Channel One\n"
        "http://example.com/one.ts\n");

    const M3UParser::Result result = M3UParser::parse(content);

    QCOMPARE(result.channels.size(), 1);
    QCOMPARE(result.skippedLines, 0);
    const Channel &c = result.channels.first();
    QCOMPARE(c.name, QStringLiteral("Channel One"));
    QCOMPARE(c.groupTitle, QStringLiteral("News"));
    QCOMPARE(c.tvgId, QStringLiteral("1"));
    QCOMPARE(c.url.toString(), QStringLiteral("http://example.com/one.ts"));
    QVERIFY(c.isValid());
}

void TestM3UParser::parsesPlainM3U()
{
    const QString content = QStringLiteral(
        "http://example.com/sports_live.ts\n"
        "http://example.com/movies_1.ts\n");

    const M3UParser::Result result = M3UParser::parse(content);

    QCOMPARE(result.channels.size(), 2);
    QCOMPARE(result.channels.at(0).name,
             QStringLiteral("sports live.ts")); // underscores become spaces
    QCOMPARE(result.channels.at(1).name,
             QStringLiteral("movies 1.ts"));
}

void TestM3UParser::readsAttributes()
{
    const QString content = QStringLiteral(
        "#EXTM3U\n"
        "#EXTINF:-1 tvg-id=\"id2022\" tvg-name=\"News HD\" "
        "tvg-logo=\"http://example.com/logo.png\" "
        "tvg-language=\"English\" tvg-country=\"US\" group-title=\"News\",Breaking News\n"
        "http://example.com/news.ts\n");

    const M3UParser::Result result = M3UParser::parse(content);

    QCOMPARE(result.channels.size(), 1);
    const Channel &c = result.channels.first();
    QCOMPARE(c.name, QStringLiteral("Breaking News"));
    QCOMPARE(c.tvgId, QStringLiteral("id2022"));
    QCOMPARE(c.tvgName, QStringLiteral("News HD"));
    QCOMPARE(c.logoUrl, QStringLiteral("http://example.com/logo.png"));
    QCOMPARE(c.language, QStringLiteral("English"));
    QCOMPARE(c.country, QStringLiteral("US"));
    QCOMPARE(c.groupTitle, QStringLiteral("News"));
}

void TestM3UParser::extgrOverridesGroup()
{
    const QString content = QStringLiteral(
        "#EXTM3U\n"
        "#EXTINF:-1 tvg-id=\"x\",My Channel\n"
        "#EXTGRP:Movies\n"
        "http://example.com/movie.ts\n");

    const M3UParser::Result result = M3UParser::parse(content);

    QCOMPARE(result.channels.size(), 1);
    QCOMPARE(result.channels.first().groupTitle, QStringLiteral("Movies"));
}

void TestM3UParser::nameFallsBackToAttributes()
{
    const QString content = QStringLiteral(
        "#EXTINF:0 tvg-id=\"42\" tvg-name=\"Alpha Sport\" group-title=\"Sports\"\n"
        "http://example.com/a.m3u8\n");

    const M3UParser::Result result = M3UParser::parse(content);

    QCOMPARE(result.channels.size(), 1);
    const Channel &c = result.channels.first();
    QCOMPARE(c.name, QStringLiteral("Alpha Sport"));
    QCOMPARE(c.tvgId, QStringLiteral("42"));
}

void TestM3UParser::handlesCrLfAndWhitespace()
{
    const QString content = QStringLiteral(
        "#EXTM3U\r\n"
        "#EXTINF:-1 group-title=\"Drama\",My Show\r\n"
        "  http://example.com/show.ts  \r\n");

    const M3UParser::Result result = M3UParser::parse(content);

    QCOMPARE(result.channels.size(), 1);
    QCOMPARE(result.channels.first().url.toString(),
             QStringLiteral("http://example.com/show.ts"));
}

void TestM3UParser::skipsMalformedContent()
{
    const QString content = QStringLiteral(
        "#EXTM3U\n"
        "#EXTVLCOPT:http-referrer=ignored\n"
        "this is not a url\n"
        "#EXTINF:-1,Ok\n"
        "http://example.com/ok.ts\n");

    const M3UParser::Result result = M3UParser::parse(content);

    QCOMPARE(result.channels.size(), 1);
    QCOMPARE(result.skippedLines, 1);
    QVERIFY(!result.warnings.isEmpty());
}

void TestM3UParser::nameCanContainQuotedCommas()
{
    const QString content = QStringLiteral(
        "#EXTINF:-1 tvg-name=\"Talk, News\",The Late Show\n"
        "http://example.com/late.ts\n");

    const M3UParser::Result result = M3UParser::parse(content);

    QCOMPARE(result.channels.size(), 1);
    QCOMPARE(result.channels.first().name, QStringLiteral("The Late Show"));
    QCOMPARE(result.channels.first().tvgName, QStringLiteral("Talk, News"));
}

void TestM3UParser::stripsBom()
{
    const QString content = QString::fromUtf8(
        "\xEF\xBB\xBF#EXTM3U\n"
        "#EXTINF:-1,With BOM\n"
        "http://example.com/bom.ts\n");

    const M3UParser::Result result = M3UParser::parse(content);

    QCOMPARE(result.channels.size(), 1);
    QCOMPARE(result.channels.first().name, QStringLiteral("With BOM"));
}

QTEST_GUILESS_MAIN(TestM3UParser)
#include "test_m3uparser.moc"