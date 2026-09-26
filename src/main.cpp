#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QLockFile>
#include <QtGlobal>

#include "core/AppInfo.h"
#include "core/Logger.h"
#include "app/Application.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QApplication::setOrganizationName(AppInfo::ORG_NAME);
    QApplication::setOrganizationDomain(QStringLiteral("copper.tv"));
    QApplication::setApplicationName(AppInfo::APP_NAME);
    QApplication::setApplicationDisplayName(AppInfo::APP_DISPLAY_NAME);
    QApplication::setApplicationVersion(AppInfo::APP_VERSION);
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/assets/app.ico")));

    // Single-instance guard: a second launch exits immediately (the stale
    // lock from a crashed process is reclaimed automatically) instead of
    // racing the first instance over the database and window state.
    QLockFile singleInstanceLock(QDir::temp().filePath(
        QStringLiteral("%1-%2.lock")
            .arg(AppInfo::APP_NAME,
                 qEnvironmentVariable("USERNAME", QStringLiteral("user")))));
    if (!singleInstanceLock.tryLock(100))
        return EXIT_SUCCESS;

    // Centralize crash-guard logging as early as possible.
    Logger::instance().install();

    int result = EXIT_FAILURE;
    {
        Application application;
        if (application.init()) {
            // Basic top-level exception guard: match installed message handler
            // so the log captures startup failures before the event loop.
            result = application.exec();
        }
    }

    Logger::instance().flush();
    Logger::instance().uninstall();
    return result;
}