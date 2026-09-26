#include "ui/ControlBar.h"

#include <QHBoxLayout>
#include <QSlider>
#include <QToolButton>

namespace {
QString speakerIcon(bool muted)
{
    return QString(QChar::fromUcs4(muted ? 0x1F507 : 0x1F50A));
}

QToolButton *makeTransportButton(const QString &glyph, const QString &name,
                                 const QString &tooltip)
{
    auto *button = new QToolButton;
    button->setText(glyph);
    button->setObjectName(QStringLiteral("transportButton"));
    button->setToolTip(tooltip);
    button->setAccessibleName(name);
    button->setAccessibleDescription(tooltip);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    button->setFixedSize(36, 32);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}
} // namespace

ControlBar::ControlBar(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(6);

    m_prev = makeTransportButton(QStringLiteral("\u23EE"), tr("Previous channel"),
                                 tr("Previous channel (Ctrl+Left)"));
    m_play = makeTransportButton(QStringLiteral("\u25B6"), tr("Play or pause"),
                                 tr("Play / Pause (Space)"));
    m_stop = makeTransportButton(QStringLiteral("\u23F9"), tr("Stop"), tr("Stop (Ctrl+S)"));
    m_next = makeTransportButton(QStringLiteral("\u23ED"), tr("Next channel"),
                                 tr("Next channel (Ctrl+Right)"));

    layout->addWidget(m_prev);
    layout->addWidget(m_play);
    layout->addWidget(m_stop);
    layout->addWidget(m_next);
    layout->addSpacing(12);

    m_mute = makeTransportButton(speakerIcon(false), tr("Mute"),
                                 tr("Toggle mute (M)"));
    m_mute->setFixedSize(36, 32);
    layout->addWidget(m_mute);

    m_volume = new QSlider(Qt::Horizontal, this);
    m_volume->setRange(0, 100);
    m_volume->setValue(70);
    m_volume->setFixedWidth(120);
    m_volume->setToolTip(tr("Volume"));
    m_volume->setAccessibleName(tr("Volume"));
    layout->addWidget(m_volume);

    m_fullscreen = makeTransportButton(QStringLiteral("\u26F6"), tr("Fullscreen"),
                                       tr("Toggle fullscreen (F)"));
    layout->addStretch(1);
    layout->addWidget(m_fullscreen);

    connect(m_prev, &QToolButton::clicked, this, &ControlBar::previousRequested);
    connect(m_play, &QToolButton::clicked, this, &ControlBar::playPauseRequested);
    connect(m_stop, &QToolButton::clicked, this, &ControlBar::stopRequested);
    connect(m_next, &QToolButton::clicked, this, &ControlBar::nextRequested);
    connect(m_mute, &QToolButton::clicked, this, [this]() {
        emit muteToggled(!m_muted);
    });
    connect(m_volume, &QSlider::valueChanged, this, &ControlBar::volumeChanged);
    connect(m_fullscreen, &QToolButton::clicked, this,
            &ControlBar::fullscreenRequested);
}

void ControlBar::setPlaying(bool playing)
{
    m_playing = playing;
    m_play->setText(playing ? QStringLiteral("\u2759\u2759")
                            : QStringLiteral("\u25B6"));
    m_play->setToolTip(playing ? tr("Pause (Space)") : tr("Play (Space)"));
}

void ControlBar::setVolume(int volume)
{
    m_volume->setValue(volume);
}

void ControlBar::setMuted(bool muted)
{
    m_muted = muted;
    m_mute->setText(muted ? speakerIcon(true) : speakerIcon(false));
}

void ControlBar::setPlaybackAvailable(bool available)
{
    m_available = available;
    const bool enabled = available;
    m_prev->setEnabled(enabled);
    m_play->setEnabled(enabled);
    m_stop->setEnabled(enabled);
    m_next->setEnabled(enabled);
}