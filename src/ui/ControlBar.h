#pragma once

#include <QWidget>

class QSlider;
class QToolButton;

// Playback transport + volume + fullscreen strip below the video.
class ControlBar : public QWidget
{
    Q_OBJECT
public:
    explicit ControlBar(QWidget *parent = nullptr);

    void setPlaying(bool playing);
    void setVolume(int volume);
    void setMuted(bool muted);
    void setPlaybackAvailable(bool available);

signals:
    void playPauseRequested();
    void stopRequested();
    void previousRequested();
    void nextRequested();
    void volumeChanged(int volume);
    void muteToggled(bool muted);
    void fullscreenRequested();

private:
    QToolButton *m_prev = nullptr;
    QToolButton *m_play = nullptr;
    QToolButton *m_stop = nullptr;
    QToolButton *m_next = nullptr;
    QToolButton *m_mute = nullptr;
    QToolButton *m_fullscreen = nullptr;
    QSlider *m_volume = nullptr;
    bool m_playing = false;
    bool m_muted = false;
    bool m_available = false;
};