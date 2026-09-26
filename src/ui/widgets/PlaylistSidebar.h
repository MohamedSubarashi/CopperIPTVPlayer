#pragma once

#include <QWidget>

class GroupListModel;
class GroupListWidget;
class PlaylistListModel;
class QLabel;
class QListView;

// Left sidebar: playlist list on top, channel groups below.
class PlaylistSidebar : public QWidget
{
    Q_OBJECT
public:
    explicit PlaylistSidebar(QWidget *parent = nullptr);

    void setPlaylistModel(PlaylistListModel *model);
    void setGroupModel(GroupListModel *model);
    void selectPlaylistById(int id);
    void selectGroup(const QString &key);

    PlaylistListModel *playlistModel() const { return m_playlistModel; }
    GroupListModel *groupModel() const { return m_groupModel; }
    QListView *playlistView() const { return m_playlistView; }
    GroupListWidget *groupWidget() const { return m_groupWidget; }

signals:
    void playlistActivated(int playlistId);
    void playlistSelectionChanged(int playlistId);
    void groupActivated(const QString &groupKey);
    void removePlaylistRequested(int playlistId);
    void refreshPlaylistRequested(int playlistId);
    void addPlaylistRequested();
    void managePlaylistsRequested();

private:
    void showPlaylistMenu(const QPoint &pos);
    int currentPlaylistId() const;

    PlaylistListModel *m_playlistModel = nullptr;
    GroupListModel *m_groupModel = nullptr;
    QListView *m_playlistView = nullptr;
    GroupListWidget *m_groupWidget = nullptr;
};