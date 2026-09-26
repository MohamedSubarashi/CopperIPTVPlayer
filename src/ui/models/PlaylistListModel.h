#pragma once

#include <QAbstractListModel>
#include <QVector>

#include "models/Playlist.h"

// Playlist list for the sidebar; surfaces name, channel count, and whether the
// playlist is remote (decorated with a small indicator in the view).
class PlaylistListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        PlaylistRole = Qt::UserRole + 1,
        NameRole,
        CountRole,
        SourceTypeRole,
        SourceRole
    };

    explicit PlaylistListModel(QObject *parent = nullptr);

    void setPlaylists(const QVector<Playlist> &playlists);
    const QVector<Playlist> &playlists() const { return m_playlists; }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    QVector<Playlist> m_playlists;
};