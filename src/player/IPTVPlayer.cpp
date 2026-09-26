#include "player/IPTVPlayer.h"

#include <QAudioOutput>
#include <QVideoWidget>

#include "core/Logger.h"

IPTVPlayer::IPTVPlayer(QObject *parent)
    : QObject(parent),
      m_player(new QMediaPlayer(this)),
      m_audioOutput(new QAudioOutput(this))
{
    m_player->setAudioOutput(m_audioOutput);

    connect(m_player, &QMediaPlayer::errorOccurred, this,
            &IPTVPlayer::onPlayerError);
    connect(m_player, &QMediaPlayer::playbackStateChanged, this,
            &IPTVPlayer::onStateChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus status) {
                emit mediaStatusChanged(static_cast<int>(status));
            });
}

void IPTVPlayer::setVideoOutput(QVideoWidget *widget)
{
    m_videoWidget = widget;
    if (widget) {
        m_player->setVideoOutput(widget);
        widget->show();
    }
}

QVideoWidget *IPTVPlayer::videoOutput() const
{
    return m_videoWidget;
}

void IPTVPlayer::play(const QUrl &url)
{
    if (url.isEmpty() || !url.isValid()) {
        emit playbackError(tr("The stream URL is invalid."),
                           static_cast<int>(ErrorCategory::InvalidUrl));
        return;
    }

    if (m_url != url) {
        m_url = url;
        if (m_player->playbackState() != QMediaPlayer::StoppedState)
            m_player->stop();
        m_player->setSource(url);
        emit sourceChanged(url);
    }
    m_player->play();
}

void IPTVPlayer::pause()
{
    m_player->pause();
}

void IPTVPlayer::stop()
{
    m_player->stop();
    // Release the stream so a stopped channel does not keep buffering.
    if (!m_url.isEmpty()) {
        m_url.clear();
        m_player->setSource(QUrl());
    }
    emit playbackStopped();
}

void IPTVPlayer::togglePlayPause()
{
    switch (m_player->playbackState()) {
    case QMediaPlayer::PlayingState:
        m_player->pause();
        break;
    case QMediaPlayer::PausedState:
        m_player->play();
        break;
    default:
        // No source loaded yet: nothing sensible to toggle.
        break;
    }
}

void IPTVPlayer::setVolume(int volume)
{
    m_audioOutput->setVolume(qBound(0.0, volume / 100.0, 1.0));
}

void IPTVPlayer::setMuted(bool muted)
{
    m_audioOutput->setMuted(muted);
}

int IPTVPlayer::volume() const
{
    return qRound(m_audioOutput->volume() * 100.0);
}

bool IPTVPlayer::isMuted() const
{
    return m_audioOutput->isMuted();
}

QMediaPlayer::PlaybackState IPTVPlayer::playbackState() const
{
    return m_player->playbackState();
}

QUrl IPTVPlayer::currentUrl() const
{
    return m_url;
}

bool IPTVPlayer::isLiveLoading() const
{
    return m_player->mediaStatus() == QMediaPlayer::LoadingMedia ||
           m_player->mediaStatus() == QMediaPlayer::BufferingMedia;
}

void IPTVPlayer::onPlayerError(QMediaPlayer::Error error,
                               const QString &errorString)
{
    QString friendly;
    ErrorCategory category = ErrorCategory::Backend;
    mapError(error, errorString, &friendly, &category);
    qWarning().noquote()
        << "[player] error" << static_cast<int>(error) << errorString;
    emit playbackError(friendly, static_cast<int>(category));
}

void IPTVPlayer::onStateChanged(QMediaPlayer::PlaybackState state)
{
    switch (state) {
    case QMediaPlayer::PlayingState:
        emit playbackStarted();
        break;
    case QMediaPlayer::PausedState:
        emit playbackPaused();
        break;
    case QMediaPlayer::StoppedState:
        emit playbackStopped();
        break;
    }
    emit playbackStateChanged(state);
}

void IPTVPlayer::mapError(QMediaPlayer::Error error, const QString &errorString,
                          QString *friendly, ErrorCategory *category) const
{
    Q_UNUSED(errorString)
    switch (error) {
    case QMediaPlayer::ResourceError:
        *friendly = tr("Unable to play this channel.\n\nThe stream may be "
                       "offline or unavailable.");
        *category = ErrorCategory::Unavailable;
        break;
    case QMediaPlayer::FormatError:
        *friendly = tr("This stream uses a format that is not supported.");
        *category = ErrorCategory::UnsupportedMedia;
        break;
    case QMediaPlayer::NetworkError:
        *friendly = tr("A network error occurred while connecting to the "
                       "stream. Check your connection.");
        *category = ErrorCategory::Network;
        break;
    case QMediaPlayer::AccessDeniedError:
        *friendly = tr("Access to this stream was denied.");
        *category = ErrorCategory::Unavailable;
        break;
    case QMediaPlayer::NoError:
        *friendly = tr("The stream could not be played.");
        *category = ErrorCategory::Backend;
        break;
    default:
        *friendly = tr("The stream could not be played. It may be offline or "
                       "use an unsupported format.");
        *category = ErrorCategory::Backend;
        break;
    }
}