#include "ui/widgets/VideoPlayerWidget.h"

#include <QLabel>
#include <QMouseEvent>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QVideoWidget>

VideoPlayerWidget::VideoPlayerWidget(QWidget *parent)
    : QWidget(parent)
{
    m_stack = new QStackedWidget(this);

    // --- Welcome page -----------------------------------------------------
    auto *welcome = new QWidget(m_stack);
    auto *welcomeLayout = new QVBoxLayout(welcome);
    welcomeLayout->addStretch();

    auto *logoLabel = new QLabel(welcome);
    logoLabel->setPixmap(QPixmap(QStringLiteral(":/assets/app.png"))
                             .scaled(128, 128, Qt::KeepAspectRatio,
                                     Qt::SmoothTransformation));
    logoLabel->setAlignment(Qt::AlignCenter);

    auto *title = new QLabel(tr("Welcome to Copper IPTV Player"), welcome);
    title->setObjectName(QStringLiteral("welcomeTitle"));
    title->setAlignment(Qt::AlignCenter);

    auto *hint = new QLabel(
        tr("No IPTV playlist has been added yet.\nAdd a local M3U/M3U8 "
           "playlist or a playlist from a URL."),
        welcome);
    hint->setObjectName(QStringLiteral("emptyHint"));
    hint->setAlignment(Qt::AlignCenter);

    auto *addButton = new QPushButton(tr("Add Playlist"), welcome);
    addButton->setAccessibleName(tr("Add a new playlist"));
    addButton->setFixedWidth(220);
    connect(addButton, &QPushButton::clicked, this,
            &VideoPlayerWidget::addPlaylistRequested);

    welcomeLayout->addWidget(logoLabel, 0, Qt::AlignHCenter);
    welcomeLayout->addSpacing(8);
    welcomeLayout->addWidget(title);
    welcomeLayout->addSpacing(6);
    welcomeLayout->addWidget(hint);
    welcomeLayout->addSpacing(16);
    welcomeLayout->addWidget(addButton, 0, Qt::AlignHCenter);
    welcomeLayout->addStretch();

    // --- Loading page -----------------------------------------------------
    auto *loading = new QWidget(m_stack);
    auto *loadingLayout = new QVBoxLayout(loading);
    loadingLayout->addStretch();
    auto *spinner = new QProgressBar(loading);
    spinner->setRange(0, 0);
    spinner->setTextVisible(false);
    spinner->setFixedWidth(240);
    spinner->setFixedHeight(6);
    m_loadingLabel = new QLabel(tr("Loading channel..."), loading);
    m_loadingLabel->setObjectName(QStringLiteral("emptyHint"));
    m_loadingLabel->setAlignment(Qt::AlignCenter);
    loadingLayout->addWidget(spinner, 0, Qt::AlignHCenter);
    loadingLayout->addSpacing(10);
    loadingLayout->addWidget(m_loadingLabel);
    loadingLayout->addStretch();

    // --- Video page -------------------------------------------------------
    m_video = new QVideoWidget(m_stack);
    m_video->setAspectRatioMode(Qt::KeepAspectRatio);
    m_video->setStyleSheet(QStringLiteral("background-color: #000000;"));

    // --- Error page -------------------------------------------------------
    auto *errorPage = new QWidget(m_stack);
    auto *errorLayout = new QVBoxLayout(errorPage);
    errorLayout->addStretch();
    m_errorTitle = new QLabel(tr("Playback error"), errorPage);
    m_errorTitle->setObjectName(QStringLiteral("errorTitle"));
    m_errorTitle->setAlignment(Qt::AlignCenter);
    m_errorDetail = new QLabel(errorPage);
    m_errorDetail->setObjectName(QStringLiteral("emptyHint"));
    m_errorDetail->setAlignment(Qt::AlignCenter);
    m_errorDetail->setWordWrap(true);
    errorLayout->addWidget(m_errorTitle);
    errorLayout->addSpacing(6);
    errorLayout->addWidget(m_errorDetail);
    errorLayout->addStretch();

    m_stack->addWidget(welcome);
    m_stack->addWidget(loading);
    m_stack->addWidget(m_video);
    m_stack->addWidget(errorPage);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_stack);

    m_backButton = new QPushButton(tr("\u2190 Back"), this);
    m_backButton->setAccessibleName(tr("Exit full screen"));
    m_backButton->setToolTip(tr("Exit full screen (Esc)"));
    m_backButton->setStyleSheet(QStringLiteral(
        "background-color: rgba(20, 20, 20, 180); color: #ffffff;"
        "border: 1px solid rgba(255, 255, 255, 60); border-radius: 4px;"
        "padding: 6px 14px;"));
    m_backButton->setCursor(Qt::PointingHandCursor);
    m_backButton->setVisible(false);
    connect(m_backButton, &QPushButton::clicked, this,
            &VideoPlayerWidget::backRequested);

    showWelcome();
}

QVideoWidget *VideoPlayerWidget::videoSurface() const
{
    return m_video;
}

void VideoPlayerWidget::showWelcome()
{
    m_current = View::Welcome;
    m_stack->setCurrentIndex(0);
}

void VideoPlayerWidget::showLoading(const QString &channelName)
{
    m_current = View::Loading;
    m_loadingLabel->setText(
        channelName.isEmpty() ? tr("Loading channel...")
                              : tr("Loading channel... %1").arg(channelName));
    m_stack->setCurrentIndex(1);
}

void VideoPlayerWidget::showVideo()
{
    m_current = View::Video;
    m_stack->setCurrentIndex(2);
}

void VideoPlayerWidget::showError(const QString &title, const QString &detail)
{
    m_current = View::Error;
    m_errorTitle->setText(title);
    m_errorDetail->setText(detail);
    m_stack->setCurrentIndex(3);
}

void VideoPlayerWidget::setFullscreenMode(bool visible)
{
    m_fullscreenMode = visible;
    m_backButton->setVisible(visible);
    if (visible)
        m_backButton->raise();
}

void VideoPlayerWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_backButton)
        m_backButton->move(12, 12);
}

void VideoPlayerWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    event->accept();
    if (m_current == View::Video)
        emit fullscreenRequested();
}