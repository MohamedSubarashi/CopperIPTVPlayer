#pragma once

#include <QAbstractListModel>
#include <QSet>
#include <QString>
#include <QVector>

#include "models/Channel.h"

// Group/category list shown in the sidebar. The first two entries are virtual
// groups ("All Channels", "Favorites"); the rest are real groups derived from
// channels' group-title values.
class GroupListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Type { All, Favorites, Group };

    struct Item
    {
        Type type = Group;
        QString name;           // display name (localized for All/Favorites)
        QString key;            // stable key ("", "favorites", or group name)
        int count = 0;
    };

    enum Roles {
        TypeRole = Qt::UserRole + 1,
        NameRole,
        CountRole,
        KeyRole,
        IsFavoritesRole
    };

    explicit GroupListModel(QObject *parent = nullptr);

    void setData(const QVector<Channel> &channels,
                 const QSet<QString> &favoriteUrls);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    const Item &itemAt(int row) const;
    int findRowByKey(const QString &key) const;
    int allRow() const;
    int favoritesRow() const;

private:
    QVector<Item> m_items;
};