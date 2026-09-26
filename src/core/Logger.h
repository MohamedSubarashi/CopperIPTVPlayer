#pragma once

#include <QFile>
#include <QMutex>
#include <QRecursiveMutex>
#include <QString>

// File-backed logger installed via qInstallMessageHandler.
// Logs are written to <AppDataLocation>/logs/copper-iptv-player.log and are
// rotated when they grow past ~1 MB. Sensitive payloads must never be passed
// to the log macros.
class Logger
{
public:
    ~Logger();

    static Logger &instance();

    // Installs the message handler. Safe to call during startup.
    void install();
    // Removes the message handler and flushes the file.
    void uninstall();

    QString logFilePath() const;
    void flush();

    static void writeLog(QtMsgType type, const QMessageLogContext &context,
                         const QString &message);

private:
    Logger();
    Q_DISABLE_COPY(Logger)

    bool openLogFile();

    // Logging is inherently re-entrant: install()/uninstall() emit log lines
    // that route through writeLog(), which also takes the mutex, so it must be
    // recursive or the first startup log deadlocks the main thread.
    QRecursiveMutex m_mutex;
    QString m_filePath;
    QFile m_file;
    bool m_installed = false;
};