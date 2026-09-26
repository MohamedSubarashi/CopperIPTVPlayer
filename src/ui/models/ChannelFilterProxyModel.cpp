#include "ui/models/ChannelFilterProxyModel.h"

#include "models/Channel.h"
#include "ui/models/ChannelListModel.h"

namespace {
const QString kFavoritesKey = QStringLiteral("favorites");
}

ChannelFilterProxyModel::ChannelFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

void ChannelFilterProxyModel::setGroup(const QString &groupKey)
{
    if (m_group == groupKey)
        return;
    m_group = groupKey;
    refilter();
}

void ChannelFilterProxyModel::setSearchText(const QString &text)
{
    if (m_search == text)
        return;
    m_search = text;
    refilter();
}

void ChannelFilterProxyModel::refilter()
{
    beginFilterChange();
    endFilterChange();
    if (m_alphaSort)
        sort(0, Qt::AscendingOrder);
}

void ChannelFilterProxyModel::setAlphabeticalSort(bool enabled)
{
    if (m_alphaSort == enabled)
        return;
    m_alphaSort = enabled;
    if (enabled) {
        setDynamicSortFilter(true);
        sort(0, Qt::AscendingOrder);
    } else {
        setDynamicSortFilter(false);
        sort(-1, Qt::AscendingOrder);
    }
}

bool ChannelFilterProxyModel::lessThan(const QModelIndex &left,
                                       const QModelIndex &right) const
{
    if (!m_alphaSort)
        return QSortFilterProxyModel::lessThan(left, right);
    const Channel a =
        left.data(ChannelListModel::ChannelRole).value<Channel>();
    const Channel b =
        right.data(ChannelListModel::ChannelRole).value<Channel>();
    return QString::compare(a.name, b.name, Qt::CaseInsensitive) < 0;
}

bool ChannelFilterProxyModel::isFavoritesGroup() const
{
    return m_group == kFavoritesKey;
}

bool ChannelFilterProxyModel::filterAcceptsRow(int sourceRow,
                                               const QModelIndex &sourceParent) const
{
    if (sourceParent.isValid())
        return true;

    const QAbstractItemModel *source = sourceModel();
    if (!source)
        return false;

    const QModelIndex idx = source->index(sourceRow, 0);
    const Channel channel = idx.data(ChannelListModel::ChannelRole).value<Channel>();

    if (!m_group.isEmpty()) {
        if (m_group == kFavoritesKey) {
            if (!channel.isFavorite)
                return false;
        } else if (channel.groupTitle.compare(m_group, Qt::CaseInsensitive) != 0) {
            return false;
        }
    }

    return ChannelUtils::matchesSearch(channel, m_search);
}