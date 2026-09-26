#pragma once

#include <QDialog>

#include "models/Channel.h"

class ChannelLogoLoader;
class QFormLayout;
class QLabel;

// Show details about a single channel, with a badge for favorites.
class ChannelInfoDialog : public QDialog
{
    Q_OBJECT
public:
    ChannelInfoDialog(const Channel &channel, bool isFavorite,
                      ChannelLogoLoader *logoLoader, QWidget *parent = nullptr);

private:
    void fillRow(QFormLayout *layout, const QString &label, const QString &value);

    Channel m_channel;
    bool m_isFavorite = false;
    ChannelLogoLoader *m_logoLoader = nullptr;
    QLabel *m_logoLabel = nullptr;
};