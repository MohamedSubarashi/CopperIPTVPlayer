#pragma once

#include <QMediaPlayer>
#include <QObject>
#include <QUrl>

class QAudioOutput;
class QVideoWidget;

// Isolates Qt Multimedia playback from the rest of the app. Maps backend
// errors to user-friendly categories; the UI only ever sees friendly strings
// plus a category for icons/logging.
class IPTVPlayer : public QObject
{
    Q_OBJECT
public:
    enum class ErrorCategory {
        None = 0,
        InvalidUrl,
        Network,
        UnsupportedMedia,
        Unavailable,
        Backend
    };
    Q_ENUM(ErrorCategory)

    explicit IPTVPlayer(QObject *parent = nullptr);

    void setVideoOutput(QVideoWidget *widget);
    QVideoWidget *videoOutput() const;

    void play(const QUrl &url);
    void pause();
    void stop();
    void togglePlayPause();

    void setVolume(int volume);
    void setMuted(bool muted);
    int volume() const;
    bool isMuted() const;

    QMediaPlayer::PlaybackState playbackState() const;
    QUrl currentUrl() const;
    bool isLiveLoading() const;

signals:
    void playbackStarted();
    void playbackPaused();
    void playbackStopped();
    void playbackStateChanged(QMediaPlayer::PlaybackState state);
    void playbackError(const QString &message, int category);
    void sourceChanged(const QUrl &url);
    void mediaStatusChanged(int status);

private slots:
    void onPlayerError(QMediaPlayer::Error error, const QString &errorString);
    void onStateChanged(QMediaPlayer::PlaybackState state);

private:
    void mapError(QMediaPlayer::Error error, const QString &errorString,
                  QString *friendly, ErrorCategory *category) const;

    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audioOutput = nullptr;
    QVideoWidget *m_videoWidget = nullptr;
    QUrl m_url;
};