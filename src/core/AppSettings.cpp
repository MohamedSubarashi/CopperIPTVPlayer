#include "core/AppSettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

#include "core/AppInfo.h"
#include "core/Constants.h"

namespace {
constexpr auto kThemeDark = "ui/themeDark";
constexpr auto kWindowGeometry = "window/geometry";
constexpr auto kWindowState = "window/state";
constexpr auto kSplitterState = "window/splitter";
constexpr auto kVolume = "playback/volume";
constexpr auto kMuted = "playback/muted";
constexpr auto kAutoplaySelected = "playback/autoplay";
constexpr auto kResumeOnStart = "playback/resumeOnStart";
constexpr auto kStartWithWindows = "general/startWithWindows";
constexpr auto kRememberLastChannel = "general/rememberLastChannel";
constexpr auto kRememberWindowState = "general/rememberWindowState";
constexpr auto kConfirmPlaylistRemoval = "general/confirmPlaylistRemoval";
constexpr auto kSidebarVisible = "interface/sidebarVisible";
constexpr auto kChannelListVisible = "interface/channelListVisible";
constexpr auto kCompactChannelList = "interface/compactRows";
constexpr auto kSortChannelsAlphabetically = "interface/sortAtoZ";
constexpr auto kNetworkTimeoutMs = "network/timeoutMs";
constexpr auto kDownloadTimeoutMs = "network/downloadTimeoutMs";
constexpr auto kUserAgent = "network/userAgent";
constexpr auto kAutoRefreshMinutes = "network/autoRefreshMinutes";
constexpr auto kIgnoreTlsErrors = "network/ignoreTlsErrors";
constexpr auto kLastPlaylistId = "last/playlistId";
constexpr auto kLastChannelUrl = "last/channelUrl";
constexpr auto kGithubUrl = "links/github";
constexpr auto kDocsUrl = "links/docs";
} // namespace

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
{
    m_settings = new QSettings(this);
}

bool AppSettings::darkTheme() const
{
    return m_settings->value(kThemeDark, true).toBool();
}

void AppSettings::setDarkTheme(bool dark)
{
    if (darkTheme() == dark)
        return;
    m_settings->setValue(kThemeDark, dark);
    emit themeChanged(dark);
}

QByteArray AppSettings::windowGeometry() const
{
    return m_settings->value(kWindowGeometry).toByteArray();
}

void AppSettings::setWindowGeometry(const QByteArray &geometry)
{
    m_settings->setValue(kWindowGeometry, geometry);
}

QByteArray AppSettings::windowState() const
{
    return m_settings->value(kWindowState).toByteArray();
}

void AppSettings::setWindowState(const QByteArray &state)
{
    m_settings->setValue(kWindowState, state);
}

QByteArray AppSettings::splitterState() const
{
    return m_settings->value(kSplitterState).toByteArray();
}

void AppSettings::setSplitterState(const QByteArray &state)
{
    m_settings->setValue(kSplitterState, state);
}

int AppSettings::volume() const
{
    int v = m_settings->value(kVolume, AppConstants::kDefaultVolume).toInt();
    return qBound(AppConstants::kMinVolume, v, AppConstants::kMaxVolume);
}

void AppSettings::setVolume(int volume)
{
    volume = qBound(AppConstants::kMinVolume, volume, AppConstants::kMaxVolume);
    m_settings->setValue(kVolume, volume);
}

bool AppSettings::muted() const
{
    return m_settings->value(kMuted, false).toBool();
}

void AppSettings::setMuted(bool muted)
{
    m_settings->setValue(kMuted, muted);
}

bool AppSettings::autoplaySelected() const
{
    return m_settings->value(kAutoplaySelected, true).toBool();
}

void AppSettings::setAutoplaySelected(bool enabled)
{
    m_settings->setValue(kAutoplaySelected, enabled);
}

bool AppSettings::resumeOnStart() const
{
    return m_settings->value(kResumeOnStart, true).toBool();
}

void AppSettings::setResumeOnStart(bool enabled)
{
    m_settings->setValue(kResumeOnStart, enabled);
}

bool AppSettings::startWithWindows() const
{
    return m_settings->value(kStartWithWindows, false).toBool();
}

void AppSettings::setStartWithWindows(bool enabled)
{
    m_settings->setValue(kStartWithWindows, enabled);
    QSettings runKey(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                     QSettings::NativeFormat);
    if (enabled) {
        const QString exe = QCoreApplication::applicationFilePath();
        runKey.setValue(AppInfo::APP_NAME + QLatin1String("_"),
                        QLatin1Char('"') + QDir::toNativeSeparators(exe) +
                            QLatin1Char('"'));
    } else {
        runKey.remove(AppInfo::APP_NAME + QLatin1String("_"));
    }
}

bool AppSettings::rememberLastChannel() const
{
    return m_settings->value(kRememberLastChannel, true).toBool();
}

void AppSettings::setRememberLastChannel(bool enabled)
{
    m_settings->setValue(kRememberLastChannel, enabled);
}

bool AppSettings::rememberWindowState() const
{
    return m_settings->value(kRememberWindowState, true).toBool();
}

void AppSettings::setRememberWindowState(bool enabled)
{
    m_settings->setValue(kRememberWindowState, enabled);
}

bool AppSettings::confirmPlaylistRemoval() const
{
    return m_settings->value(kConfirmPlaylistRemoval, true).toBool();
}

void AppSettings::setConfirmPlaylistRemoval(bool enabled)
{
    m_settings->setValue(kConfirmPlaylistRemoval, enabled);
}

bool AppSettings::sidebarVisible() const
{
    return m_settings->value(kSidebarVisible, true).toBool();
}

void AppSettings::setSidebarVisible(bool visible)
{
    m_settings->setValue(kSidebarVisible, visible);
    emit interfaceChanged();
}

bool AppSettings::channelListVisible() const
{
    return m_settings->value(kChannelListVisible, true).toBool();
}

void AppSettings::setChannelListVisible(bool visible)
{
    m_settings->setValue(kChannelListVisible, visible);
    emit interfaceChanged();
}

bool AppSettings::compactChannelList() const
{
    return m_settings->value(kCompactChannelList, false).toBool();
}

void AppSettings::setCompactChannelList(bool compact)
{
    m_settings->setValue(kCompactChannelList, compact);
    emit interfaceChanged();
}

bool AppSettings::sortChannelsAlphabetically() const
{
    return m_settings->value(kSortChannelsAlphabetically, false).toBool();
}

void AppSettings::setSortChannelsAlphabetically(bool enabled)
{
    m_settings->setValue(kSortChannelsAlphabetically, enabled);
    emit interfaceChanged();
}

int AppSettings::networkTimeoutMs() const
{
    return m_settings->value(kNetworkTimeoutMs, AppConstants::kDefaultNetworkTimeoutMs).toInt();
}

void AppSettings::setNetworkTimeoutMs(int ms)
{
    m_settings->setValue(kNetworkTimeoutMs, ms);
}

int AppSettings::downloadTimeoutMs() const
{
    return m_settings->value(kDownloadTimeoutMs, AppConstants::kDefaultDownloadTimeoutMs).toInt();
}

void AppSettings::setDownloadTimeoutMs(int ms)
{
    m_settings->setValue(kDownloadTimeoutMs, ms);
}

QString AppSettings::userAgent() const
{
    const QString stored = m_settings->value(kUserAgent, QString()).toString();
    if (!stored.isEmpty())
        return stored;
    return QStringLiteral("%1/%2 (Qt %3)")
        .arg(AppInfo::APP_DISPLAY_NAME, AppInfo::APP_VERSION,
             QStringLiteral(QT_VERSION_STR));
}

void AppSettings::setUserAgent(const QString &ua)
{
    m_settings->setValue(kUserAgent, ua);
}

int AppSettings::autoRefreshMinutes() const
{
    return m_settings->value(kAutoRefreshMinutes, AppConstants::kAutoRefreshDisabled).toInt();
}

void AppSettings::setAutoRefreshMinutes(int minutes)
{
    m_settings->setValue(kAutoRefreshMinutes, minutes);
}

bool AppSettings::ignoreTlsErrors() const
{
    return m_settings->value(kIgnoreTlsErrors, false).toBool();
}

void AppSettings::setIgnoreTlsErrors(bool ignore)
{
    m_settings->setValue(kIgnoreTlsErrors, ignore);
}

int AppSettings::lastPlaylistId() const
{
    return m_settings->value(kLastPlaylistId, -1).toInt();
}

void AppSettings::setLastPlaylistId(int id)
{
    m_settings->setValue(kLastPlaylistId, id);
}

QString AppSettings::lastChannelUrl() const
{
    return m_settings->value(kLastChannelUrl, QString()).toString();
}

void AppSettings::setLastChannelUrl(const QString &url)
{
    m_settings->setValue(kLastChannelUrl, url);
}

QString AppSettings::githubUrl() const
{
    const QString stored = m_settings->value(kGithubUrl, QString()).toString();
    if (!stored.isEmpty())
        return stored;
    return AppInfo::APP_GITHUB_URL;
}

void AppSettings::setGithubUrl(const QString &url)
{
    m_settings->setValue(kGithubUrl, url);
}

QString AppSettings::docsUrl() const
{
    const QString stored = m_settings->value(kDocsUrl, QString()).toString();
    if (!stored.isEmpty())
        return stored;
    return AppInfo::APP_DOCS_URL;
}

void AppSettings::setDocsUrl(const QString &url)
{
    m_settings->setValue(kDocsUrl, url);
}

void AppSettings::sync()
{
    m_settings->sync();
}