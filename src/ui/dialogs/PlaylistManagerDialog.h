#pragma once

#include <QDialog>

class PlaylistManager;
class AppSettings;
class QTableWidget;

// Manage the list of playlists: add / edit / refresh / remove.
class PlaylistManagerDialog : public QDialog
{
    Q_OBJECT
public:
    PlaylistManagerDialog(PlaylistManager *manager, AppSettings *settings,
                          QWidget *parent = nullptr);

    int lastAddedPlaylistId() const { return m_lastAddedId; }
    int lastRemovedPlaylistId() const { return m_lastRemovedId; }

public slots:
    void refreshTable();

private slots:
    void onAdd();
    void onEdit();
    void onRefresh();
    void onRemove();

private:
    int selectedPlaylistId() const;

    PlaylistManager *m_manager = nullptr;
    AppSettings *m_settings = nullptr;
    QTableWidget *m_table = nullptr;
    int m_lastAddedId = -1;
    int m_lastRemovedId = -1;
};