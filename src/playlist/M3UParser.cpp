#include "playlist/M3UParser.h"

#include <QRegularExpression>
#include <QUrl>

#include "models/Channel.h"

namespace {

const QRegularExpression &attributeRegex()
{
    static const QRegularExpression re(
        QStringLiteral(R"attr(([A-Za-z0-9_][A-Za-z0-9_.-]*)\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s]+)))attr"));
    return re;
}

} // namespace

M3UParser::Result M3UParser::parse(const QString &rawContent)
{
    Result result;

    QString content = rawContent;
    if (content.startsWith(QChar(0xFEFF)))
        content.remove(0, 1);

    const QStringList rawLines = content.split(QLatin1Char('\n'));
    if (rawLines.isEmpty()) {
        result.warnings.append(QStringLiteral("Playlist is empty."));
        return result;
    }

    QStringList lines;
    lines.reserve(rawLines.size());
    for (QString line : rawLines) {
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        line = line.trimmed();
        lines.append(line);
    }

    Pending pending;
    bool afterExtinf = false;
    QString bareNameLine;

    const int lineCount = lines.size();
    for (int i = 0; i < lineCount; ++i) {
        const QString &line = lines.at(i);

        if (line.isEmpty())
            continue;
        if (line.startsWith(QStringLiteral("#EXTM3U")))
            continue; // header
        if (line.startsWith(QStringLiteral("#EXTINF"))) {
            pending = parseExtinf(line);
            afterExtinf = true;
            bareNameLine.clear();
            continue;
        }
        if (line.startsWith(QStringLiteral("#EXTGRP"))) {
            pending.groupOverride = line.mid(8).trimmed();
            continue;
        }
        if (line.startsWith(QLatin1Char('#'))) {
            // Other directives (e.g. #EXTVLCOPT) are ignored for playback.
            continue;
        }

        // Non-comment, non-directive line.
        if (ChannelUtils::isStreamUrl(line)) {
            Channel channel;
            channel.url = QUrl(line);
            if (!pending.name.isEmpty()) {
                channel.name = pending.name;
            } else if (afterExtinf && !bareNameLine.isEmpty()) {
                channel.name = bareNameLine;
            } else {
                channel.name = deriveNameFromUrl(line);
            }
            channel.groupTitle = !pending.groupOverride.isEmpty()
                                     ? pending.groupOverride
                                     : pending.attrs.value(QStringLiteral("group-title"));
            applyAttributes(&channel, pending.attrs);
            result.channels.append(channel);

            pending = Pending();
            afterExtinf = false;
            bareNameLine.clear();
            continue;
        }

        // Bare text line. After an EXTINF without an inline name this is the
        // channel name (3-line entry form). Otherwise it is orphaned content.
        if (afterExtinf && pending.name.isEmpty()) {
            pending.name = line;
        } else if (!afterExtinf) {
            ++result.skippedLines;
            result.warnings.append(QStringLiteral("Line %1 has no preceding #EXTINF entry: '%2'")
                                       .arg(i + 1)
                                       .arg(line.left(120)));
        }
        bareNameLine = line;
    }

    return result;
}

M3UParser::Pending M3UParser::parseExtinf(const QString &line)
{
    Pending pending;
    // "#EXTINF:<duration> <attributes>[,<name>]" or "#EXTINF:<duration>,<name>"
    QString rest = line.mid(8).trimmed();
    const int sepSpace = rest.indexOf(QLatin1Char(' '));
    const int sepComma = rest.indexOf(QLatin1Char(','));
    if (sepSpace < 0)
        rest = sepComma >= 0 ? rest.mid(sepComma + 1).trimmed() : QString();
    else if (sepComma < 0 || sepSpace < sepComma)
        rest = rest.mid(sepSpace + 1).trimmed();

    QString name;
    splitNameFromInf(&rest, &name);
    pending.name = name;
    pending.attrs = parseAttributes(rest);

    if (pending.name.isEmpty()) {
        const QString tvgName = pending.attrs.value(QStringLiteral("tvg-name"));
        const QString tvgId = pending.attrs.value(QStringLiteral("tvg-id"));
        pending.name = !tvgName.isEmpty() ? tvgName : tvgId;
    }

    if (pending.name.length() > 500)
        pending.name = pending.name.left(500);

    return pending;
}

void M3UParser::splitNameFromInf(QString *attributesText, QString *name)
{
    // The channel name is everything after the LAST comma that is not inside
    // quotes. This keeps quoted attributes with embedded commas intact.
    bool inQuotes = false;
    int lastComma = -1;
    const QString &text = *attributesText;
    for (int i = text.size() - 1; i >= 0; --i) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('"'))
            inQuotes = !inQuotes;
        else if (c == QLatin1Char(',') && !inQuotes) {
            lastComma = i;
            break;
        }
    }

    if (lastComma < 0) {
        name->clear();
        return;
    }

    *name = text.mid(lastComma + 1).trimmed();
    *attributesText = text.left(lastComma).trimmed();
}

QHash<QString, QString> M3UParser::parseAttributes(const QString &text)
{
    QHash<QString, QString> attrs;
    auto match = attributeRegex().globalMatch(text);
    while (match.hasNext()) {
        const QRegularExpressionMatch m = match.next();
        const QString key = m.captured(1);
        QString value = m.captured(2);
        if (value.isEmpty())
            value = m.captured(3);
        if (value.isEmpty())
            value = m.captured(4);
        attrs.insert(key, value.trimmed());
    }
    return attrs;
}

void M3UParser::applyAttributes(Channel *channel, const QHash<QString, QString> &attrs)
{
    auto value = [&attrs](const char *key) {
        return attrs.value(QLatin1String(key));
    };
    channel->tvgId = value("tvg-id");
    channel->tvgName = value("tvg-name");
    channel->logoUrl = value("tvg-logo");
    channel->language = value("tvg-language");
    channel->country = value("tvg-country");
    channel->attributes = attrs;
}

QString M3UParser::deriveNameFromUrl(const QString &url)
{
    const QUrl u(url);
    QString last = u.path().section(QLatin1Char('/'), -1);
    if (last.isEmpty())
        last = u.host();
    last.replace(QLatin1Char('_'), QLatin1Char(' '));
    last = last.trimmed();
    if (last.isEmpty())
        return url.left(80);
    return last;
}