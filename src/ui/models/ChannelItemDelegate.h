#pragma once

#include <QStyledItemDelegate>

class ChannelLogoLoader;

// Custom row painter: async logo + name + group + favorite star. Fixed row
// heights keep large playlists (10k+ channels) fast.
class ChannelItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    ChannelItemDelegate(const ChannelLogoLoader *logoLoader,
                        QObject *parent = nullptr);

    void setCompact(bool compact);

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

private:
    const ChannelLogoLoader *m_logoLoader = nullptr;
    bool m_compact = false;
};