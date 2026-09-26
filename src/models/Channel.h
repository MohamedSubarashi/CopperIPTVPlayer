#pragma once

#include <QHash>
#include <QMetaType>
#include <QString>
#include <QUrl>
#include <QVector>

#include "models/ChannelGroup.h"

// A single IPTV channel parsed from a playlist. Value type designed to be
// cheap to copy into Qt model roles and to sort/filter freely.
struct Channel
{
    QString name;
    QUrl url;
    QString groupTitle;
    QString tvgId;
    QString tvgName;
    QString logoUrl;
    QString language;
    QString country;
    QHash<QString, QString> attributes;

    int playlistId = -1;
    bool isFavorite = false;

    bool isValid() const { return url.isValid() && !url.isEmpty(); }
    QString streamUrl() const { return url.toString(); }
};

namespace ChannelUtils {

// Returns the canonical group for a channel (never empty).
QString groupFor(const Channel &channel);

// Builds the ordered group list (name + count) for a set of channels.
QVector<ChannelGroup> buildGroups(const QVector<Channel> &channels);

// Case-insensitive search across name, tvg-id, group, language, country.
bool matchesSearch(const Channel &channel, const QString &query);

// Redacts credentials embedded in a URL so they are never shown in the UI.
QString maskedUrl(const QString &url);

// Simple heuristic: is this an HTTP(S) stream URL?
bool isStreamUrl(const QString &candidate);

} // namespace ChannelUtils

Q_DECLARE_METATYPE(Channel)