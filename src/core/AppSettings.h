#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class QSettings;

// Typed wrapper around QSettings. All persistent UI/behavior preferences live
// here. Runtime data (playlists, favorites, channel snapshots) lives in SQLite.
class AppSettings : public QObject
{
    Q_OBJECT
public:
    explicit AppSettings(QObject *parent = nullptr);

    // Theme
    bool darkTheme() const;
    void setDarkTheme(bool dark);

    // Window state
    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray &geometry);
    QByteArray windowState() const;
    void setWindowState(const QByteArray &state);
    QByteArray splitterState() const;
    void setSplitterState(const QByteArray &state);

    // Playback
    int volume() const;
    void setVolume(int volume);
    bool muted() const;
    void setMuted(bool muted);
    bool autoplaySelected() const;
    void setAutoplaySelected(bool enabled);
    bool resumeOnStart() const;
    void setResumeOnStart(bool enabled);

    // General
    bool startWithWindows() const;
    void setStartWithWindows(bool enabled);
    bool rememberLastChannel() const;
    void setRememberLastChannel(bool enabled);
    bool rememberWindowState() const;
    void setRememberWindowState(bool enabled);
    bool confirmPlaylistRemoval() const;
    void setConfirmPlaylistRemoval(bool enabled);

    // Interface
    bool sidebarVisible() const;
    void setSidebarVisible(bool visible);
    bool channelListVisible() const;
    void setChannelListVisible(bool visible);
    bool compactChannelList() const;
    void setCompactChannelList(bool compact);
    bool sortChannelsAlphabetically() const;
    void setSortChannelsAlphabetically(bool enabled);

    // Network
    int networkTimeoutMs() const;
    void setNetworkTimeoutMs(int ms);
    int downloadTimeoutMs() const;
    void setDownloadTimeoutMs(int ms);
    QString userAgent() const;
    void setUserAgent(const QString &ua);
    int autoRefreshMinutes() const;
    void setAutoRefreshMinutes(int minutes);
    bool ignoreTlsErrors() const;
    void setIgnoreTlsErrors(bool ignore);

    // Last selection
    int lastPlaylistId() const;
    void setLastPlaylistId(int id);
    QString lastChannelUrl() const;
    void setLastChannelUrl(const QString &url);

    // External links (override defaults from AppInfo)
    QString githubUrl() const;
    void setGithubUrl(const QString &url);
    QString docsUrl() const;
    void setDocsUrl(const QString &url);

    void sync();

signals:
    void themeChanged(bool dark);
    void interfaceChanged();

private:
    QSettings *m_settings;
};