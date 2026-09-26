#include "app/Application.h"

#include <QApplication>
#include <QDir>
#include <QStandardPaths>

#include "core/AppSettings.h"
#include "core/Logger.h"
#include "database/DatabaseManager.h"
#include "network/ChannelLogoLoader.h"
#include "network/NetworkManager.h"
#include "playlist/PlaylistManager.h"
#include "ui/MainWindow.h"

Application::Application(QObject *parent)
    : QObject(parent)
{
}

bool Application::init()
{
    Logger::instance().install();
    qInfo("Initializing %s %s",
          qPrintable(QApplication::applicationDisplayName()),
          qPrintable(QApplication::applicationVersion()));

    m_settings = new AppSettings(this);

    m_database = new DatabaseManager(this);
    const QString dataDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!dataDir.isEmpty() && !QDir().exists(dataDir))
        QDir().mkpath(dataDir);
    if (!m_database->open()) {
        qWarning() << "Database failed to open; running without persistence:"
                   << m_database->lastError();
    } else {
        qInfo("Database opened: %s", qPrintable(m_database->databasePath()));
    }

    m_network = new NetworkManager(this);
    m_network->setUserAgent(m_settings->userAgent());

    m_logoLoader = new ChannelLogoLoader(m_network, this);

    m_playlistManager = new PlaylistManager(m_database, m_network, m_settings, this);
    m_playlistManager->loadFromDatabase();
    m_playlistManager->setAutoRefreshMinutes(m_settings->autoRefreshMinutes());

    m_mainWindow = new MainWindow(m_settings, m_database, m_network,
                                  m_playlistManager, m_logoLoader);
    m_mainWindow->show();

    return true;
}

int Application::exec()
{
    Q_ASSERT(qApp);
    return qApp->exec();
}