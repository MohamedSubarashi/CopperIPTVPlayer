#include "models/Channel.h"

#include <QMap>
#include <QSet>
#include <algorithm>

#include "core/Constants.h"
#include "models/ChannelGroup.h"

namespace ChannelUtils {

QString groupFor(const Channel &channel)
{
    QString g = channel.groupTitle.trimmed();
    if (g.isEmpty())
        g = AppConstants::kGroupUncategorized;
    return g;
}

QVector<ChannelGroup> buildGroups(const QVector<Channel> &channels)
{
    QStringList order;
    QMap<QString, int> counts;

    for (const Channel &c : channels) {
        const QString g = groupFor(c);
        if (!counts.contains(g))
            order.append(g);
        ++counts[g];
    }

    QVector<ChannelGroup> groups;
    groups.reserve(order.size());
    for (const QString &name : std::as_const(order))
        groups.append(ChannelGroup(name, counts.value(name, 0)));
    return groups;
}

bool matchesSearch(const Channel &channel, const QString &query)
{
    if (query.isEmpty())
        return true;
    const QString q = query.toCaseFolded();
    auto contains = [&q](const QString &value) {
        return value.toCaseFolded().contains(q);
    };
    return contains(channel.name) || contains(channel.tvgId) ||
           contains(channel.tvgName) || contains(channel.groupTitle) ||
           contains(channel.language) || contains(channel.country);
}

QString maskedUrl(const QString &url)
{
    QUrl u(url);
    if (!u.userInfo().isEmpty())
        u.setUserInfo(QString());
    return u.toString(QUrl::FullyEncoded);
}

bool isStreamUrl(const QString &candidate)
{
    const QString lower = candidate.toCaseFolded().trimmed();
    return lower.startsWith(QStringLiteral("http://")) ||
           lower.startsWith(QStringLiteral("https://"));
}

} // namespace ChannelUtils