#pragma once

#include <QAbstractListModel>
#include <QVector>

#include "models/Channel.h"

// Flat channel list used by the channel view and the filter proxy.
class ChannelListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        ChannelRole = Qt::UserRole + 1,
        NameRole,
        LogoRole,
        GroupRole,
        FavoriteRole,
        UrlRole,
        TvgIdRole,
        LanguageRole,
        CountryRole
    };

    explicit ChannelListModel(QObject *parent = nullptr);

    void setChannels(const QVector<Channel> &channels);

    const QVector<Channel> &channels() const { return m_channels; }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    QVector<Channel> m_channels;
};