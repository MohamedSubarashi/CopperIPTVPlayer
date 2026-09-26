#include "player/PlaybackController.h"

#include <QAbstractItemView>
#include <QMediaPlayer>

#include "core/AppSettings.h"
#include "player/IPTVPlayer.h"
#include "ui/models/ChannelFilterProxyModel.h"
#include "ui/models/ChannelListModel.h"

PlaybackController::PlaybackController(IPTVPlayer *player, AppSettings *settings,
                                       QObject *parent)
    : QObject(parent), m_player(player), m_settings(settings)
{
    connect(m_player, &IPTVPlayer::playbackError, this,
            &PlaybackController::playbackError);
    connect(m_player, &IPTVPlayer::playbackStateChanged, this,
            [this](QMediaPlayer::PlaybackState state) {
                emit playStateChanged(state == QMediaPlayer::PlayingState);
            });
    connect(m_player, &IPTVPlayer::playbackStopped, this, [this]() {
        if (!m_player->currentUrl().isEmpty())
            emit statusMessage(tr("Playback stopped."));
    });
}

void PlaybackController::setViews(QAbstractItemView *view,
                                  ChannelFilterProxyModel *proxy)
{
    m_view = view;
    m_proxy = proxy;
}

bool PlaybackController::playRow(int proxyRow)
{
    if (!m_proxy || proxyRow < 0 || proxyRow >= m_proxy->rowCount())
        return false;

    const QModelIndex idx = m_proxy->index(proxyRow, 0);
    const Channel channel = idx.data(ChannelListModel::ChannelRole).value<Channel>();
    if (!channel.isValid()) {
        emit playbackError(tr("Unable to play this channel."));
        return false;
    }

    m_currentRow = proxyRow;
    if (m_view)
        m_view->setCurrentIndex(idx);
    startPlayback(channel);
    return true;
}

bool PlaybackController::playCurrentSelection()
{
    if (m_view && m_view->currentIndex().isValid())
        return playRow(m_view->currentIndex().row());

    if (m_currentChannel.isValid()) {
        startPlayback(m_currentChannel);
        return true;
    }

    emit statusMessage(tr("No channel selected."));
    return false;
}

bool PlaybackController::togglePlayPause()
{
    if (!m_currentChannel.isValid()) {
        emit statusMessage(tr("No channel selected."));
        return false;
    }
    // After stop() the player releases its source; re-load it on demand.
    if (m_player->playbackState() == QMediaPlayer::StoppedState ||
        m_player->currentUrl().isEmpty()) {
        m_player->play(m_currentChannel.url);
        emit nowPlaying(m_currentChannel.name);
        return true;
    }
    m_player->togglePlayPause();
    return true;
}

void PlaybackController::stop()
{
    m_player->stop();
    emit statusMessage(tr("Stopped."));
}

void PlaybackController::playNext()
{
    move(1);
}

void PlaybackController::playPrevious()
{
    move(-1);
}

bool PlaybackController::selectAndPlay(const Channel &channel)
{
    if (!channel.isValid())
        return false;

    int row = -1;
    if (m_proxy) {
        const int count = m_proxy->rowCount();
        for (int i = 0; i < count; ++i) {
            const QModelIndex idx = m_proxy->index(i, 0);
            const Channel candidate =
                idx.data(ChannelListModel::ChannelRole).value<Channel>();
            if (candidate.url == channel.url) {
                row = i;
                break;
            }
        }
    }

    m_currentRow = row;
    if (m_view && row >= 0)
        m_view->setCurrentIndex(m_proxy->index(row, 0));
    startPlayback(channel);
    return true;
}

bool PlaybackController::hasCurrent() const
{
    return m_currentChannel.isValid();
}

Channel PlaybackController::currentChannel() const
{
    return m_currentChannel;
}

QString PlaybackController::currentChannelName() const
{
    return m_currentChannel.name;
}

QUrl PlaybackController::currentUrl() const
{
    return m_currentChannel.url;
}

void PlaybackController::startPlayback(const Channel &channel)
{
    const bool sameChannel = m_player->currentUrl() == channel.url;
    const bool alreadyPlaying =
        m_player->playbackState() == QMediaPlayer::PlayingState;

    m_currentChannel = channel;

    if (sameChannel && alreadyPlaying) {
        emit nowPlaying(channel.name);
        emit currentChannelChanged(channel);
        return;
    }

    emit statusMessage(tr("Loading channel..."));
    emit nowPlaying(channel.name);
    emit currentChannelChanged(channel);
    // IPTVPlayer stops the previous stream internally when the source changes.
    m_player->play(channel.url);
}

void PlaybackController::move(int step)
{
    if (!m_proxy || m_proxy->rowCount() == 0) {
        emit statusMessage(tr("No channels available."));
        return;
    }

    int base = m_currentRow;
    if (base < 0 && m_view)
        base = m_view->currentIndex().row();

    int target = base + step;
    if (base < 0)
        target = 0; // nothing selected yet: start at the first channel
    target = qBound(0, target, m_proxy->rowCount() - 1);

    if (target == base && m_currentChannel.isValid() && step != 0) {
        // At the edge of the list: stay put.
        emit statusMessage(tr("Reached the end of the channel list."));
        return;
    }
    playRow(target);
}