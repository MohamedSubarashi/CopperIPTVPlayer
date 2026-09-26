#pragma once

#include <QObject>
#include <QUrl>

#include "models/Channel.h"

class AppSettings;
class ChannelFilterProxyModel;
class IPTVPlayer;
class QAbstractItemView;

// UI-side controller coordinating the channel list, IPTVPlayer, and settings.
// Guarantees single-stream playback (each play stops any previous stream) and
// implements prev/next navigation over the currently filtered channel order.
class PlaybackController : public QObject
{
    Q_OBJECT
public:
    PlaybackController(IPTVPlayer *player, AppSettings *settings,
                       QObject *parent = nullptr);

    void setViews(QAbstractItemView *view, ChannelFilterProxyModel *proxy);

    bool playRow(int proxyRow);
    bool playCurrentSelection();
    bool togglePlayPause();
    void stop();
    void playNext();
    void playPrevious();

    // Plays a specific channel, locating its row in the filtered model.
    bool selectAndPlay(const Channel &channel);

    bool hasCurrent() const;
    Channel currentChannel() const;
    QString currentChannelName() const;
    QUrl currentUrl() const;
    int currentRow() const { return m_currentRow; }

signals:
    void statusMessage(const QString &message);
    void nowPlaying(const QString &channelName);
    void playbackError(const QString &message);
    void playStateChanged(bool playing);
    void currentChannelChanged(const Channel &channel);

private:
    void startPlayback(const Channel &channel);
    void move(int step);

    IPTVPlayer *m_player = nullptr;
    AppSettings *m_settings = nullptr;
    QAbstractItemView *m_view = nullptr;
    ChannelFilterProxyModel *m_proxy = nullptr;
    Channel m_currentChannel;
    int m_currentRow = -1;
};