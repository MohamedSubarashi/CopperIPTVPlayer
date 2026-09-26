#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>

#include "core/AppInfo.h"

// A user-managed playlist record (source + metadata). Channels themselves are
// held separately by the PlaylistManager / DatabaseManager.
class Playlist
{
public:
    enum class SourceType { Local, Remote };

    bool isRemote() const { return type == SourceType::Remote; }
    bool isEnabled() const { return enabled; }

    QString displaySource() const { return source; }
    QString lastRefreshDisplay() const
    {
        if (lastRefresh.isNull())
            return QString();
        return lastRefresh.toString(Qt::ISODate);
    }

    int id = -1;
    QString name;
    SourceType type = SourceType::Local;
    QString source;
    bool enabled = true;
    QDateTime lastRefresh;
    int channelCount = 0;
};

Q_DECLARE_METATYPE(Playlist)