#include "ui/models/PlaylistListModel.h"

PlaylistListModel::PlaylistListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void PlaylistListModel::setPlaylists(const QVector<Playlist> &playlists)
{
    beginResetModel();
    m_playlists = playlists;
    endResetModel();
}

int PlaylistListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_playlists.size();
}

QVariant PlaylistListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_playlists.size())
        return QVariant();

    const Playlist &p = m_playlists.at(index.row());
    switch (role) {
    case PlaylistRole:
        return QVariant::fromValue(p);
    case NameRole:
        return p.name;
    case CountRole:
        return p.channelCount;
    case SourceTypeRole:
        return p.isRemote() ? QStringLiteral("remote") : QStringLiteral("local");
    case SourceRole:
        return p.source;
    case Qt::DisplayRole:
        return p.channelCount > 0
                   ? QStringLiteral("%1 (%2)").arg(p.name).arg(p.channelCount)
                   : p.name;
    case Qt::ToolTipRole:
        return p.source;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> PlaylistListModel::roleNames() const
{
    return {
        {PlaylistRole, "playlist"},
        {NameRole, "name"},
        {CountRole, "count"},
        {SourceTypeRole, "sourceType"},
        {SourceRole, "source"},
    };
}