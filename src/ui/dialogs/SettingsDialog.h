#pragma once

#include <QDialog>

class AppSettings;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QTabWidget;

// Settings dialog bound directly to AppSettings. Values are written on OK.
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(AppSettings *settings, QWidget *parent = nullptr);

private:
    void applySettings();
    void writeAutoRefresh(QComboBox *combo);

    AppSettings *m_settings = nullptr;
    QTabWidget *m_tabs = nullptr;

    // General
    QCheckBox *m_startWithWindows = nullptr;
    QCheckBox *m_rememberLastChannel = nullptr;
    QCheckBox *m_rememberWindowState = nullptr;
    QCheckBox *m_confirmRemoval = nullptr;

    // Playback
    QSpinBox *m_defaultVolume = nullptr;
    QCheckBox *m_autoplay = nullptr;
    QCheckBox *m_resumeOnStart = nullptr;

    // Interface
    QCheckBox *m_sidebarVisible = nullptr;
    QCheckBox *m_channelListVisible = nullptr;
    QCheckBox *m_compactRows = nullptr;
    QComboBox *m_theme = nullptr;

    // Network
    QSpinBox *m_timeout = nullptr;
    QSpinBox *m_downloadTimeout = nullptr;
    QLineEdit *m_userAgent = nullptr;
    QComboBox *m_autoRefresh = nullptr;
    QCheckBox *m_ignoreTls = nullptr;
};