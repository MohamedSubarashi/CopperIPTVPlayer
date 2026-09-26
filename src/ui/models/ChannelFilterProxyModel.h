#pragma once

#include <QSortFilterProxyModel>
#include <QString>

// Filters the channel model by group and by instant search text. Both filters
// apply without touching the source model, keeping large playlists responsive.
class ChannelFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit ChannelFilterProxyModel(QObject *parent = nullptr);

    // "" = all channels, "favorites" = favorite channels, otherwise a group
    // name as stored in Channel::groupTitle.
    void setGroup(const QString &groupKey);
    void setSearchText(const QString &text);

    // Re-applies the current filters (used after the source model changes).
    void refilter();

    // Toggles alphabetical sorting by channel name (case-insensitive). Off
    // restores the source (playlist) order.
    void setAlphabeticalSort(bool enabled);
    bool alphabeticalSort() const { return m_alphaSort; }

    QString group() const { return m_group; }
    QString searchText() const { return m_search; }
    bool isFavoritesGroup() const;

protected:
    bool filterAcceptsRow(int sourceRow,
                          const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left,
                  const QModelIndex &right) const override;

private:
    QString m_group;
    QString m_search;
    bool m_alphaSort = false;
};