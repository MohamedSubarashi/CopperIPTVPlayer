#pragma once

#include <QObject>

class AppSettings;
class ChannelLogoLoader;
class DatabaseManager;
class MainWindow;
class NetworkManager;
class PlaylistManager;

// Owns the application's service objects and the main window. Keeps main()
// minimal and gives a single place to order startup/shutdown.
class Application : public QObject
{
    Q_OBJECT
public:
    explicit Application(QObject *parent = nullptr);

    // Initializes services; returns false only on unrecoverable failure.
    bool init();
    // Blocks until the event loop finishes (application retcode).
    int exec();

private:
    AppSettings *m_settings = nullptr;
    DatabaseManager *m_database = nullptr;
    NetworkManager *m_network = nullptr;
    PlaylistManager *m_playlistManager = nullptr;
    ChannelLogoLoader *m_logoLoader = nullptr;
    MainWindow *m_mainWindow = nullptr;
};