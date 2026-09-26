#pragma once

#include <QString>

// A single channel category/group with its channel count.
class ChannelGroup
{
public:
    ChannelGroup() = default;
    ChannelGroup(const QString &name, int count)
        : m_name(name), m_count(count)
    {
    }

    QString name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }
    int channelCount() const { return m_count; }
    void setChannelCount(int count) { m_count = count; }

private:
    QString m_name;
    int m_count = 0;
};