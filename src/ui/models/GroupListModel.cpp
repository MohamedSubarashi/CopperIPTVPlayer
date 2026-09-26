#include "ui/models/GroupListModel.h"

#include "models/ChannelGroup.h"

GroupListModel::GroupListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void GroupListModel::setData(const QVector<Channel> &channels,
                             const QSet<QString> &favoriteUrls)
{
    beginResetModel();
    m_items.clear();

    Item all;
    all.type = All;
    all.name = tr("All Channels");
    all.key = QString();
    all.count = channels.size();
    m_items.append(all);

    Item favs;
    favs.type = Favorites;
    favs.name = tr("Favorites");
    favs.key = QStringLiteral("favorites");
    for (const Channel &c : channels) {
        if (c.isFavorite || favoriteUrls.contains(c.url.toString()))
            ++favs.count;
    }
    m_items.append(favs);

    const QVector<ChannelGroup> groups = ChannelUtils::buildGroups(channels);
    for (const ChannelGroup &g : groups) {
        Item item;
        item.type = Group;
        item.name = g.name();
        item.key = g.name();
        item.count = g.channelCount();
        m_items.append(item);
    }

    endResetModel();
}

int GroupListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_items.size();
}

QVariant GroupListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return QVariant();

    const Item &item = m_items.at(index.row());
    switch (role) {
    case TypeRole:
        return static_cast<int>(item.type);
    case NameRole:
        return item.name;
    case CountRole:
        return item.count;
    case KeyRole:
        return item.key;
    case IsFavoritesRole:
        return item.type == Favorites;
    case Qt::DisplayRole:
        return item.count > 0
                   ? QStringLiteral("%1 (%2)").arg(item.name).arg(item.count)
                   : item.name;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> GroupListModel::roleNames() const
{
    return {
        {TypeRole, "groupType"},
        {NameRole, "name"},
        {CountRole, "count"},
        {KeyRole, "key"},
        {IsFavoritesRole, "isFavorites"},
    };
}

const GroupListModel::Item &GroupListModel::itemAt(int row) const
{
    static const Item empty;
    if (row < 0 || row >= m_items.size())
        return empty;
    return m_items.at(row);
}

int GroupListModel::findRowByKey(const QString &key) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).key == key)
            return i;
    }
    return -1;
}

int GroupListModel::allRow() const
{
    return findRowByKey(QString());
}

int GroupListModel::favoritesRow() const
{
    return findRowByKey(QStringLiteral("favorites"));
}