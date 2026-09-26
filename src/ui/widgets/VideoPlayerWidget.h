#pragma once

#include <QWidget>

class QLabel;
class QProgressBar;
class QPushButton;
class QResizeEvent;
class QStackedWidget;
class QVideoWidget;

// Stacked video area: welcome / loading / video / error states. Keeps playback
// (QVideoWidget) isolated from UI-state presentation.
class VideoPlayerWidget : public QWidget
{
    Q_OBJECT
public:
    enum class View { Welcome, Loading, Video, Error };

    explicit VideoPlayerWidget(QWidget *parent = nullptr);

    QVideoWidget *videoSurface() const;

    void showWelcome();
    void showLoading(const QString &channelName);
    void showVideo();
    void showError(const QString &title, const QString &detail);
    View currentView() const { return m_current; }

    void setFullscreenMode(bool visible);

signals:
    void fullscreenRequested();
    void backRequested();
    void addPlaylistRequested();

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QStackedWidget *m_stack = nullptr;
    QVideoWidget *m_video = nullptr;
    QLabel *m_loadingLabel = nullptr;
    QLabel *m_errorTitle = nullptr;
    QLabel *m_errorDetail = nullptr;
    QPushButton *m_backButton = nullptr;
    bool m_fullscreenMode = false;
    View m_current = View::Welcome;
};