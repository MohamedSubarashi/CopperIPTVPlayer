#include "ui/models/ChannelListModel.h"

ChannelListModel::ChannelListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void ChannelListModel::setChannels(const QVector<Channel> &channels)
{
    beginResetModel();
    m_channels = channels;
    endResetModel();
}

int ChannelListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_channels.size();
}

QVariant ChannelListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_channels.size())
        return QVariant();

    const Channel &c = m_channels.at(index.row());
    switch (role) {
    case ChannelRole:
        return QVariant::fromValue(c);
    case NameRole:
        return c.name;
    case LogoRole:
        return c.logoUrl;
    case GroupRole:
        return c.groupTitle;
    case TvgIdRole:
        return c.tvgId;
    case LanguageRole:
        return c.language;
    case CountryRole:
        return c.country;
    case FavoriteRole:
        return c.isFavorite;
    case UrlRole:
        return c.url;
    case Qt::DisplayRole:
        return c.name;
    case Qt::DecorationRole:
        return QVariant(); // drawn by the custom delegate
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> ChannelListModel::roleNames() const
{
    return {
        {ChannelRole, "channel"},
        {NameRole, "name"},
        {LogoRole, "logo"},
        {GroupRole, "group"},
        {FavoriteRole, "favorite"},
        {UrlRole, "url"},
        {TvgIdRole, "tvgId"},
        {LanguageRole, "language"},
        {CountryRole, "country"},
    };
}