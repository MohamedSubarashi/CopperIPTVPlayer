#include "core/Logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QtGlobal>

#include "core/Constants.h"

Logger::Logger() = default;

Logger::~Logger()
{
    uninstall();
}

Logger &Logger::instance()
{
    static Logger logger;
    return logger;
}

bool Logger::openLogFile()
{
    const QString dataDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDir.isEmpty())
        return false;

    QDir dir(dataDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
        return false;

    const QString logsDir = QDir(dataDir).filePath(AppConstants::kLogsSubdir);
    if (!QDir().exists(logsDir) && !QDir().mkpath(logsDir))
        return false;

    m_filePath = QDir(logsDir).filePath(AppConstants::kLogFileName);

    // Rotate: if the log exceeds ~1.5 MB, shift current to ".1" and start fresh.
    const QFileInfo info(m_filePath);
    if (info.exists() && info.size() >= 1536 * 1024) {
        QFile::remove(m_filePath + QLatin1String(".1"));
        QFile::rename(m_filePath, m_filePath + QLatin1String(".1"));
    }

    m_file.setFileName(m_filePath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return false;
    return true;
}

void Logger::install()
{
    QMutexLocker locker(&m_mutex);
    if (m_installed)
        return;
    if (!openLogFile())
        return;
    qInstallMessageHandler(&Logger::writeLog);
    m_installed = true;

    qInfo("Logger initialized: %s", qPrintable(m_filePath));
    qInfo("Application startup at %s",
          qPrintable(QDateTime::currentDateTime().toString(Qt::ISODateWithMs)));
}

void Logger::uninstall()
{
    QMutexLocker locker(&m_mutex);
    if (!m_installed)
        return;
    qInfo("Application shutdown at %s",
          qPrintable(QDateTime::currentDateTime().toString(Qt::ISODateWithMs)));
    qInstallMessageHandler(nullptr);
    m_installed = false;
    m_file.flush();
    m_file.close();
}

QString Logger::logFilePath() const
{
    return m_filePath;
}

void Logger::flush()
{
    QMutexLocker locker(&m_mutex);
    m_file.flush();
}

void Logger::writeLog(QtMsgType type, const QMessageLogContext &context,
                      const QString &message)
{
    Q_UNUSED(context)
    Logger &self = instance();
    QMutexLocker locker(&self.m_mutex);
    if (!self.m_file.isOpen())
        return;

    const char *level = "INFO";
    switch (type) {
    case QtDebugMsg:
        level = "DEBUG";
        break;
    case QtInfoMsg:
        level = "INFO";
        break;
    case QtWarningMsg:
        level = "WARN";
        break;
    case QtCriticalMsg:
        level = "CRIT";
        break;
    case QtFatalMsg:
        level = "FATAL";
        break;
    }

    const QString line =
        QStringLiteral("%1 [%2] %3")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
                 QLatin1String(level), message);

    QByteArray utf8 = line.toUtf8();
    utf8.append('\n');
    self.m_file.write(utf8);
    self.m_file.flush();
}