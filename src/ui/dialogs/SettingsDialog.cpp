#include "ui/dialogs/SettingsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include "core/AppSettings.h"
#include "core/Constants.h"

namespace {

QGroupBox *makeBox(const QString &title, QWidget *parent)
{
    auto *box = new QGroupBox(title, parent);
    return box;
}

} // namespace

SettingsDialog::SettingsDialog(AppSettings *settings, QWidget *parent)
    : QDialog(parent), m_settings(settings)
{
    setWindowTitle(tr("Settings"));
    resize(600, 480);

    auto *layout = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);

    // ---- General ----
    auto *generalPage = new QWidget(m_tabs);
    auto *generalLayout = new QVBoxLayout(generalPage);
    auto *generalBox = makeBox(tr("General"), generalPage);
    auto *generalForm = new QFormLayout(generalBox);
    m_startWithWindows = new QCheckBox(generalBox);
    m_rememberLastChannel = new QCheckBox(generalBox);
    m_rememberWindowState = new QCheckBox(generalBox);
    m_confirmRemoval = new QCheckBox(generalBox);
    m_startWithWindows->setText(tr("Start Copper IPTV Player with Windows"));
    m_rememberLastChannel->setText(tr("Remember the last selected channel"));
    m_rememberWindowState->setText(tr("Remember window size and position"));
    m_confirmRemoval->setText(tr("Confirm before removing a playlist"));
    m_startWithWindows->setChecked(m_settings->startWithWindows());
    m_rememberLastChannel->setChecked(m_settings->rememberLastChannel());
    m_rememberWindowState->setChecked(m_settings->rememberWindowState());
    m_confirmRemoval->setChecked(m_settings->confirmPlaylistRemoval());
    generalForm->addRow(m_startWithWindows);
    generalForm->addRow(m_rememberLastChannel);
    generalForm->addRow(m_rememberWindowState);
    generalForm->addRow(m_confirmRemoval);
    generalLayout->addWidget(generalBox);
    generalLayout->addStretch();
    m_tabs->addTab(generalPage, tr("General"));

    // ---- Playback ----
    auto *playbackPage = new QWidget(m_tabs);
    auto *playbackLayout = new QVBoxLayout(playbackPage);
    auto *playbackBox = makeBox(tr("Playback"), playbackPage);
    auto *playbackForm = new QFormLayout(playbackBox);
    m_defaultVolume = new QSpinBox(playbackBox);
    m_defaultVolume->setRange(0, 100);
    m_defaultVolume->setSuffix(QStringLiteral("%"));
    m_defaultVolume->setValue(m_settings->volume());
    m_autoplay = new QCheckBox(tr("Auto-play the selected channel"), playbackBox);
    m_autoplay->setChecked(m_settings->autoplaySelected());
    m_resumeOnStart = new QCheckBox(
        tr("Resume the last channel on application start"), playbackBox);
    m_resumeOnStart->setChecked(m_settings->resumeOnStart());
    playbackForm->addRow(tr("Default volume"), m_defaultVolume);
    playbackForm->addRow(m_autoplay);
    playbackForm->addRow(m_resumeOnStart);
    playbackLayout->addWidget(playbackBox);
    playbackLayout->addStretch();
    m_tabs->addTab(playbackPage, tr("Playback"));

    // ---- Interface ----
    auto *interfacePage = new QWidget(m_tabs);
    auto *interfaceLayout = new QVBoxLayout(interfacePage);
    auto *interfaceBox = makeBox(tr("Interface"), interfacePage);
    auto *interfaceForm = new QFormLayout(interfaceBox);
    m_theme = new QComboBox(interfacePage);
    m_theme->addItem(tr("Dark"), true);
    m_theme->addItem(tr("Light"), false);
    const int themeIndex = m_settings->darkTheme() ? 0 : 1;
    m_theme->setCurrentIndex(themeIndex);
    m_sidebarVisible = new QCheckBox(tr("Show the playlist sidebar"), interfacePage);
    m_sidebarVisible->setChecked(m_settings->sidebarVisible());
    m_channelListVisible =
        new QCheckBox(tr("Show the channel list"), interfacePage);
    m_channelListVisible->setChecked(m_settings->channelListVisible());
    m_compactRows = new QCheckBox(tr("Compact channel rows"), interfacePage);
    m_compactRows->setChecked(m_settings->compactChannelList());
    interfaceForm->addRow(tr("Theme"), m_theme);
    interfaceForm->addRow(m_sidebarVisible);
    interfaceForm->addRow(m_channelListVisible);
    interfaceForm->addRow(m_compactRows);
    interfaceLayout->addWidget(interfaceBox);
    interfaceLayout->addStretch();
    m_tabs->addTab(interfacePage, tr("Interface"));

    // ---- Network ----
    auto *networkPage = new QWidget(m_tabs);
    auto *networkLayout = new QVBoxLayout(networkPage);
    auto *networkBox = makeBox(tr("Network"), networkPage);
    auto *networkForm = new QFormLayout(networkBox);
    m_timeout = new QSpinBox(networkBox);
    m_timeout->setRange(5, 300);
    m_timeout->setSuffix(tr(" s"));
    m_timeout->setValue(m_settings->networkTimeoutMs() / 1000);
    m_downloadTimeout = new QSpinBox(networkBox);
    m_downloadTimeout->setRange(5, 600);
    m_downloadTimeout->setSuffix(tr(" s"));
    m_downloadTimeout->setValue(m_settings->downloadTimeoutMs() / 1000);
    m_userAgent = new QLineEdit(m_settings->userAgent(), networkBox);
    m_autoRefresh = new QComboBox(networkBox);
    m_autoRefresh->addItem(tr("Refresh disabled"), AppConstants::kAutoRefreshDisabled);
    m_autoRefresh->addItem(tr("Every 15 minutes"), AppConstants::kAutoRefresh15m);
    m_autoRefresh->addItem(tr("Every 30 minutes"), AppConstants::kAutoRefresh30m);
    m_autoRefresh->addItem(tr("Every 1 hour"), AppConstants::kAutoRefresh1h);
    m_autoRefresh->addItem(tr("Every 6 hours"), AppConstants::kAutoRefresh6h);
    m_autoRefresh->addItem(tr("Every 12 hours"), AppConstants::kAutoRefresh12h);
    m_autoRefresh->addItem(tr("Every 24 hours"), AppConstants::kAutoRefresh24h);
    int refreshIndex = 0;
    for (int i = 0; i < m_autoRefresh->count(); ++i) {
        if (m_autoRefresh->itemData(i).toInt() == m_settings->autoRefreshMinutes()) {
            refreshIndex = i;
            break;
        }
    }
    m_autoRefresh->setCurrentIndex(refreshIndex);
    m_ignoreTls = new QCheckBox(
        tr("Ignore TLS certificate errors when downloading playlists"), networkBox);
    m_ignoreTls->setChecked(m_settings->ignoreTlsErrors());
    networkForm->addRow(tr("Connection timeout"), m_timeout);
    networkForm->addRow(tr("Playlist download timeout"), m_downloadTimeout);
    networkForm->addRow(tr("User-Agent"), m_userAgent);
    networkForm->addRow(tr("Automatic playlist refresh"), m_autoRefresh);
    networkForm->addRow(m_ignoreTls);
    networkLayout->addWidget(networkBox);
    networkLayout->addWidget(new QLabel(
        tr("Automatic refresh applies to remote playlists. If a refresh fails, "
           "the previously loaded channels are kept."),
        networkPage));
    networkLayout->addStretch();
    m_tabs->addTab(networkPage, tr("Network"));

    layout->addWidget(m_tabs);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
                                             QDialogButtonBox::Cancel,
                                         this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Save"));
    buttons->button(QDialogButtonBox::Ok)->setDefault(true);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        applySettings();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void SettingsDialog::applySettings()
{
    if (!m_settings)
        return;

    m_settings->setStartWithWindows(m_startWithWindows->isChecked());
    m_settings->setRememberLastChannel(m_rememberLastChannel->isChecked());
    m_settings->setRememberWindowState(m_rememberWindowState->isChecked());
    m_settings->setConfirmPlaylistRemoval(m_confirmRemoval->isChecked());

    m_settings->setVolume(m_defaultVolume->value());
    m_settings->setAutoplaySelected(m_autoplay->isChecked());
    m_settings->setResumeOnStart(m_resumeOnStart->isChecked());

    m_settings->setDarkTheme(m_theme->currentData().toBool());
    m_settings->setSidebarVisible(m_sidebarVisible->isChecked());
    m_settings->setChannelListVisible(m_channelListVisible->isChecked());
    m_settings->setCompactChannelList(m_compactRows->isChecked());

    m_settings->setNetworkTimeoutMs(m_timeout->value() * 1000);
    m_settings->setDownloadTimeoutMs(m_downloadTimeout->value() * 1000);
    m_settings->setUserAgent(m_userAgent->text().trimmed());
    m_settings->setAutoRefreshMinutes(m_autoRefresh->currentData().toInt());
    m_settings->setIgnoreTlsErrors(m_ignoreTls->isChecked());

    m_settings->sync();
}